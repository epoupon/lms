/*
 * Copyright (C) 2024 Emeric Poupon
 *
 * This file is part of LMS.
 *
 * LMS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LMS.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ScanStepAssociateReleaseImages.hpp"

#include <array>
#include <cassert>
#include <deque>
#include <set>
#include <variant>

#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "core/Path.hpp"
#include "database/Artwork.hpp"
#include "database/Db.hpp"
#include "database/Directory.hpp"
#include "database/Image.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/TrackEmbeddedImage.hpp"
#include "database/TrackEmbeddedImageId.hpp"

#include "ScanContext.hpp"

namespace lms::scanner
{
    namespace
    {
        using PreferredArtwork = std::variant<std::monostate, db::TrackEmbeddedImageId, db::ImageId>;
        bool isSameArtwork(PreferredArtwork preferredArtwork, const db::Artwork::pointer& artwork)
        {
            if (std::holds_alternative<std::monostate>(preferredArtwork))
                return !artwork;

            if (const db::TrackEmbeddedImageId* trackEmbeddedImageId = std::get_if<db::TrackEmbeddedImageId>(&preferredArtwork))
                return artwork && *trackEmbeddedImageId == artwork->getTrackEmbeddedImageId();

            if (const db::ImageId* imageId = std::get_if<db::ImageId>(&preferredArtwork))
                return artwork && *imageId == artwork->getImageId();

            return false;
        }

        std::filesystem::path toPath(db::Session& session, const db::TrackEmbeddedImageId trackEmbeddedImageId)
        {
            std::filesystem::path res;

            db::Track::FindParameters params;
            params.setEmbeddedImage(trackEmbeddedImageId);
            params.setRange(db::Range{ .offset = 0, .size = 1 });
            db::Track::find(session, params, [&](const db::Track::pointer& track) {
                res = track->getAbsoluteFilePath();
            });

            return res;
        }

        std::filesystem::path toPath(db::Session& session, const db::ImageId imageId)
        {
            db::Image::pointer image{ db::Image::find(session, imageId) };
            return image ? image->getAbsoluteFilePath() : std::filesystem::path{};
        }

        struct ReleaseImageAssociation
        {
            db::ReleaseId releaseId;
            PreferredArtwork preferredArtwork;
        };
        using ReleaseImageAssociationContainer = std::deque<ReleaseImageAssociation>;

        struct SearchReleaseImageContext
        {
            db::Session& session;
            db::ReleaseId lastRetrievedReleaseId;
            std::size_t processedReleaseCount{};
            const std::vector<std::string>& releaseImageFileNames;
        };

        db::Image::pointer findImageInDirectory(SearchReleaseImageContext& searchContext, const std::filesystem::path& directoryPath)
        {
            db::Image::pointer image;

            const db::Directory::pointer directory{ db::Directory::find(searchContext.session, directoryPath) };
            if (directory) // may not exist for releases that are split on different media libraries
            {
                for (std::string_view fileStem : searchContext.releaseImageFileNames)
                {
                    db::Image::FindParameters params;
                    params.setDirectory(directory->getId());
                    params.setFileStem(fileStem);

                    db::Image::find(searchContext.session, params, [&](const db::Image::pointer foundImg) {
                        if (!image)
                            image = foundImg;
                    });

                    if (image)
                        break;
                }
            }

            return image;
        }

        db::Image::pointer computeBestReleaseImage(SearchReleaseImageContext& searchContext, const db::Release::pointer& release)
        {
            db::Image::pointer image;

            const auto mbid{ release->getMBID() };
            if (mbid)
            {
                // Find anywhere, since it is suppoed to be unique!
                db::Image::find(searchContext.session, db::Image::FindParameters{}.setFileStem(mbid->getAsString()), [&](const db::Image::pointer foundImg) {
                    if (!image)
                        image = foundImg;
                });
            }

            if (!image)
            {
                std::set<std::filesystem::path> releasePaths;
                db::Directory::FindParameters params;
                params.setRelease(release->getId());

                db::Directory::find(searchContext.session, params, [&](const db::Directory::pointer& directory) {
                    releasePaths.insert(directory->getAbsolutePath());
                });

                // Expect layout like this:
                // Artist/Release/CD1/...
                //               /CD2/...
                //               /cover.jpg
                if (releasePaths.size() > 1)
                {
                    const std::filesystem::path releasePath{ core::pathUtils::getLongestCommonPath(std::cbegin(releasePaths), std::cend(releasePaths)) };
                    image = findImageInDirectory(searchContext, releasePath);
                }

                if (!image)
                {
                    for (const std::filesystem::path& releasePath : releasePaths)
                    {
                        image = findImageInDirectory(searchContext, releasePath);
                        if (image)
                            break;
                    }
                }
            }

            return image;
        }

        PreferredArtwork computeBestReleaseArtwork(SearchReleaseImageContext& searchContext, const db::Release::pointer& release)
        {
            const db::Image::pointer image{ computeBestReleaseImage(searchContext, release) };
            if (image)
                return PreferredArtwork{ image->getId() };

            // Fallback on embedded Front image
            db::TrackEmbeddedImageId trackEmbeddedImageId;
            {
                db::TrackEmbeddedImage::FindParameters params;
                params.setRelease(release->getId());
                params.setImageTypes({ db::ImageType::FrontCover });
                params.setSortMethod(db::TrackEmbeddedImageSortMethod::DiscNumberThenTrackNumberThenSizeDesc);
                params.setRange(db::Range{ .offset = 0, .size = 1 });

                db::TrackEmbeddedImage::find(searchContext.session, params, [&](const db::TrackEmbeddedImage::pointer& image) { trackEmbeddedImageId = image->getId(); });
            }

            if (trackEmbeddedImageId.isValid())
                return PreferredArtwork{ trackEmbeddedImageId };

            // Fallback on embedded media image
            {
                db::TrackEmbeddedImage::FindParameters params;
                params.setRelease(release->getId());
                params.setImageTypes({ db::ImageType::Media });
                params.setSortMethod(db::TrackEmbeddedImageSortMethod::DiscNumberThenTrackNumberThenSizeDesc);
                params.setRange(db::Range{ .offset = 0, .size = 1 });

                db::TrackEmbeddedImage::find(searchContext.session, params, [&](const db::TrackEmbeddedImage::pointer& image) { trackEmbeddedImageId = image->getId(); });
            }

            if (trackEmbeddedImageId.isValid())
                return PreferredArtwork{ trackEmbeddedImageId };

            return PreferredArtwork{};
        }

        bool fetchNextReleaseArtworksToUpdate(SearchReleaseImageContext& searchContext, ReleaseImageAssociationContainer& releaseImageAssociations)
        {
            const db::ReleaseId releaseId{ searchContext.lastRetrievedReleaseId };

            {
                constexpr std::size_t readBatchSize{ 100 };

                auto transaction{ searchContext.session.createReadTransaction() };

                db::Release::find(searchContext.session, searchContext.lastRetrievedReleaseId, readBatchSize, [&](const db::Release::pointer& release) {
                    const PreferredArtwork preferredArtwork{ computeBestReleaseArtwork(searchContext, release) };
                    const db::Artwork::pointer currentPreferredArtwork{ release->getPreferredArtwork() };

                    if (!isSameArtwork(preferredArtwork, currentPreferredArtwork))
                        releaseImageAssociations.push_back(ReleaseImageAssociation{ release->getId(), preferredArtwork });
                    searchContext.processedReleaseCount++;
                });
            }

            return releaseId != searchContext.lastRetrievedReleaseId;
        }

        db::Artwork::pointer getOrCreateArtworkFromTrackEmbeddedImage(db::Session& session, const db::TrackEmbeddedImageId& trackEmbeddedImageId)
        {
            db::Artwork::pointer artwork{ db::Artwork::find(session, trackEmbeddedImageId) };
            if (!artwork)
            {
                db::TrackEmbeddedImage::pointer trackEmbeddedImage{ db::TrackEmbeddedImage::find(session, trackEmbeddedImageId) };
                assert(trackEmbeddedImage);
                artwork = session.create<db::Artwork>(trackEmbeddedImage);
            }
            return artwork;
        }

        db::Artwork::pointer getOrCreateArtworkFromImage(db::Session& session, const db::ImageId& imageId)
        {
            db::Artwork::pointer artwork{ db::Artwork::find(session, imageId) };
            if (!artwork)
            {
                db::Image::pointer image{ db::Image::find(session, imageId) };
                assert(image);
                artwork = session.create<db::Artwork>(image);
            }
            return artwork;
        }

        void updateReleaseArtwork(db::Session& session, const ReleaseImageAssociation& releaseImageAssociation)
        {
            db::Release::pointer release{ db::Release::find(session, releaseImageAssociation.releaseId) };
            assert(release);

            db::Artwork::pointer artwork;

            if (const db::TrackEmbeddedImageId * trackEmbeddedImageId{ std::get_if<db::TrackEmbeddedImageId>(&releaseImageAssociation.preferredArtwork) })
            {
                artwork = getOrCreateArtworkFromTrackEmbeddedImage(session, *trackEmbeddedImageId);
                LMS_LOG(DBUPDATER, DEBUG, "Updating preferred artwork in release '" << release->getName() << "' with embedded image in track " << toPath(session, *trackEmbeddedImageId));
            }
            else if (const db::ImageId * imageId{ std::get_if<db::ImageId>(&releaseImageAssociation.preferredArtwork) })
            {
                artwork = getOrCreateArtworkFromImage(session, *imageId);
                LMS_LOG(DBUPDATER, DEBUG, "Updating preferred artwork in release '" << release->getName() << "' with image " << toPath(session, *imageId));
            }
            else
            {
                LMS_LOG(DBUPDATER, DEBUG, "Removing preferred artwork from release '" << release->getName() << "'");
            }

            release.modify()->setPreferredArtwork(artwork);
        }

        void updateReleaseImages(db::Session& session, ReleaseImageAssociationContainer& imageAssociations)
        {
            constexpr std::size_t writeBatchSize{ 20 };

            while (!imageAssociations.empty())
            {
                auto transaction{ session.createWriteTransaction() };

                for (std::size_t i{}; !imageAssociations.empty() && i < writeBatchSize; ++i)
                {
                    updateReleaseArtwork(session, imageAssociations.front());
                    imageAssociations.pop_front();
                }
            }
        }

        std::vector<std::string> constructReleaseImageFileNames()
        {
            std::vector<std::string> res;

            core::Service<core::IConfig>::get()->visitStrings("cover-preferred-file-names",
                [&res](std::string_view fileName) {
                    res.emplace_back(fileName);
                },
                { "cover", "front", "folder", "default" });

            return res;
        }

    } // namespace

    ScanStepAssociateReleaseImages::ScanStepAssociateReleaseImages(InitParams& initParams)
        : ScanStepBase{ initParams }
        , _releaseImageFileNames{ constructReleaseImageFileNames() }
    {
    }

    bool ScanStepAssociateReleaseImages::needProcess(const ScanContext& context) const
    {
        return context.stats.nbChanges() > 0;
    }

    void ScanStepAssociateReleaseImages::process(ScanContext& context)
    {
        auto& session{ _db.getTLSSession() };

        {
            auto transaction{ session.createReadTransaction() };
            context.currentStepStats.totalElems = db::Release::getCount(session);
        }

        SearchReleaseImageContext searchContext{
            .session = session,
            .lastRetrievedReleaseId = {},
            .releaseImageFileNames = _releaseImageFileNames,
        };

        ReleaseImageAssociationContainer releaseImageAssociations;
        while (fetchNextReleaseArtworksToUpdate(searchContext, releaseImageAssociations))
        {
            if (_abortScan)
                return;

            updateReleaseImages(session, releaseImageAssociations);
            context.currentStepStats.processedElems = searchContext.processedReleaseCount;
            _progressCallback(context.currentStepStats);
        }
    }
} // namespace lms::scanner
