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

#include "core/IConfig.hpp"
#include "core/IJob.hpp"
#include "core/ILogger.hpp"
#include "core/Path.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/Image.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackEmbeddedImage.hpp"

#include "JobQueue.hpp"
#include "ScanContext.hpp"

namespace lms::scanner
{
    namespace
    {
        struct ReleaseArtworkAssociation
        {
            db::ReleaseId releaseId;
            db::ArtworkId preferredArtworkId;
        };
        using ReleaseArtworkAssociationContainer = std::deque<ReleaseArtworkAssociation>;

        struct SearchReleaseArtworkParams
        {
            const std::vector<std::string>& releaseImageFileNames;
        };

        db::Artwork::pointer findImageInDirectory(db::Session& session, const SearchReleaseArtworkParams& searchParams, const std::filesystem::path& directoryPath)
        {
            db::Artwork::pointer artwork;

            const db::Directory::pointer directory{ db::Directory::find(session, directoryPath) };
            if (directory) // may not exist for releases that are split on different media libraries
            {
                for (std::string_view fileStem : searchParams.releaseImageFileNames)
                {
                    db::Image::FindParameters params;
                    params.setDirectory(directory->getId());
                    params.setFileStem(fileStem, db::Image::FindParameters::ProcessWildcards{ true }); // no need to sanitize here, user is responsible for providing sanitized file stems in conf file

                    db::Image::find(session, params, [&](const db::Image::pointer& image) {
                        if (!artwork)
                            artwork = db::Artwork::find(session, image->getId());
                    });

                    if (artwork)
                        break;
                }
            }

            return artwork;
        }

        db::Artwork::pointer computePreferredReleaseImage(db::Session& session, const SearchReleaseArtworkParams& searchParams, const db::Release::pointer& release)
        {
            db::Artwork::pointer artwork;

            const auto mbid{ release->getMBID() };
            if (mbid)
            {
                // Find anywhere, since it is suppoed to be unique!
                db::Image::find(session, db::Image::FindParameters{}.setFileStem(mbid->getAsString()), [&](const db::Image::pointer& image) {
                    if (!artwork)
                        artwork = db::Artwork::find(session, image->getId());
                });
            }

            if (!artwork)
            {
                std::set<std::filesystem::path> releasePaths;
                db::Directory::FindParameters params;
                params.setRelease(release->getId());

                db::Directory::find(session, params, [&](const db::Directory::pointer& directory) {
                    releasePaths.insert(directory->getAbsolutePath());
                });

                // Expect layout like this:
                // Artist/Release/CD1/...
                //               /CD2/...
                //               /cover.jpg
                if (releasePaths.size() > 1)
                {
                    const std::filesystem::path releasePath{ core::pathUtils::getLongestCommonPath(std::cbegin(releasePaths), std::cend(releasePaths)) };
                    artwork = findImageInDirectory(session, searchParams, releasePath);
                }

                if (!artwork)
                {
                    for (const std::filesystem::path& releasePath : releasePaths)
                    {
                        artwork = findImageInDirectory(session, searchParams, releasePath);
                        if (artwork)
                            break;
                    }
                }
            }

            return artwork;
        }

        db::Artwork::pointer computePreferredReleaseArtwork(db::Session& session, const SearchReleaseArtworkParams& searchParams, const db::Release::pointer& release)
        {
            db::Artwork::pointer artwork{ computePreferredReleaseImage(session, searchParams, release) };
            if (artwork)
                return artwork;

            // Fallback on embedded Front image
            {
                db::TrackEmbeddedImage::FindParameters params;
                params.setRelease(release->getId());
                params.setImageType(db::ImageType::FrontCover);
                params.setSortMethod(db::TrackEmbeddedImageSortMethod::DiscNumberThenTrackNumberThenSizeDesc);
                db::TrackEmbeddedImage::find(session, params, [&](const db::TrackEmbeddedImage::pointer& image) {
                    if (!artwork)
                        artwork = db::Artwork::find(session, image->getId());
                });
            }

            if (artwork)
                return artwork;

            // Fallback on embedded media image
            {
                db::TrackEmbeddedImage::FindParameters params;
                params.setRelease(release->getId());
                params.setImageType(db::ImageType::Media);
                params.setSortMethod(db::TrackEmbeddedImageSortMethod::DiscNumberThenTrackNumberThenSizeDesc);
                db::TrackEmbeddedImage::find(session, params, [&](const db::TrackEmbeddedImage::pointer& image) {
                    if (!artwork)
                        artwork = db::Artwork::find(session, image->getId());
                });
            }

            return artwork;
        }

        void updateReleasePreferredArtwork(db::Session& session, const ReleaseArtworkAssociation& releaseArtworkAssociation)
        {
            db::Release::updatePreferredArtwork(session, releaseArtworkAssociation.releaseId, releaseArtworkAssociation.preferredArtworkId);
        }

        void updateReleasePreferredArtworks(db::Session& session, ReleaseArtworkAssociationContainer& imageAssociations, bool forceFullBatch)
        {
            constexpr std::size_t writeBatchSize{ 50 };

            while ((forceFullBatch && imageAssociations.size() >= writeBatchSize) || !imageAssociations.empty())
            {
                auto transaction{ session.createWriteTransaction() };

                for (std::size_t i{}; !imageAssociations.empty() && i < writeBatchSize; ++i)
                {
                    updateReleasePreferredArtwork(session, imageAssociations.front());
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

        bool fetchNextReleaseIdRange(db::Session& session, db::ReleaseId& lastRetrievedId, db::IdRange<db::ReleaseId>& idRange)
        {
            constexpr std::size_t readBatchSize{ 100 };

            auto transaction{ session.createReadTransaction() };

            idRange = db::Release::findNextIdRange(session, lastRetrievedId, readBatchSize);
            lastRetrievedId = idRange.last;

            return idRange.isValid();
        }

        class ComputeReleaseArtworkAssociationsJob : public core::IJob
        {
        public:
            ComputeReleaseArtworkAssociationsJob(db::IDb& db, const SearchReleaseArtworkParams& searchParams, db::IdRange<db::ReleaseId> artistIdRange)
                : _db{ db }
                , _searchParams{ searchParams }
                , _artistIdRange{ artistIdRange }
            {
            }

            std::span<const ReleaseArtworkAssociation> getAssociations() const { return _associations; }
            std::size_t getProcessedReleaseCount() const { return _processedReleaseCount; }

        private:
            core::LiteralString getName() const override { return "Associate Release Artworks"; }
            void run() override
            {
                auto& session{ _db.getTLSSession() };
                auto transaction{ session.createReadTransaction() };

                db::Release::find(session, _artistIdRange, [this, &session](const db::Release::pointer& release) {
                    const db::Artwork::pointer preferredArtwork{ computePreferredReleaseArtwork(session, _searchParams, release) };

                    if (release->getPreferredArtwork() != preferredArtwork)
                    {
                        _associations.push_back(ReleaseArtworkAssociation{ release->getId(), preferredArtwork ? preferredArtwork->getId() : db::ArtworkId{} });

                        if (preferredArtwork)
                            LMS_LOG(DBUPDATER, DEBUG, "Updating preferred artwork for release '" << release->getName() << "' with image in " << preferredArtwork->getAbsoluteFilePath());
                        else
                            LMS_LOG(DBUPDATER, DEBUG, "Removing preferred artwork from release '" << release->getName() << "'");
                    }

                    _processedReleaseCount++;
                });
            }

            db::IDb& _db;
            const SearchReleaseArtworkParams& _searchParams;
            db::IdRange<db::ReleaseId> _artistIdRange;
            std::vector<ReleaseArtworkAssociation> _associations;
            std::size_t _processedReleaseCount{};
        };

    } // namespace

    ScanStepAssociateReleaseImages::ScanStepAssociateReleaseImages(InitParams& initParams)
        : ScanStepBase{ initParams }
        , _releaseImageFileNames{ constructReleaseImageFileNames() }
    {
    }

    bool ScanStepAssociateReleaseImages::needProcess(const ScanContext& context) const
    {
        return context.stats.getChangesCount() > 0;
    }

    void ScanStepAssociateReleaseImages::process(ScanContext& context)
    {
        auto& session{ _db.getTLSSession() };

        {
            auto transaction{ session.createReadTransaction() };
            context.currentStepStats.totalElems = db::Release::getCount(session);
        }

        const SearchReleaseArtworkParams searchParams{
            .releaseImageFileNames = _releaseImageFileNames,
        };

        ReleaseArtworkAssociationContainer artistArtworkAssociations;
        auto processJobsDone = [&](std::span<std::unique_ptr<core::IJob>> jobs) {
            if (_abortScan)
                return;

            for (const auto& job : jobs)
            {
                const auto& associationJob{ static_cast<const ComputeReleaseArtworkAssociationsJob&>(*job) };
                const auto& artistAssociations{ associationJob.getAssociations() };

                artistArtworkAssociations.insert(std::end(artistArtworkAssociations), std::cbegin(artistAssociations), std::cend(artistAssociations));

                context.currentStepStats.processedElems += associationJob.getProcessedReleaseCount();
            }

            updateReleasePreferredArtworks(session, artistArtworkAssociations, true);
            _progressCallback(context.currentStepStats);
        };

        JobQueue queue{ getJobScheduler(), 20, processJobsDone, 1, 0.85F };

        db::ReleaseId lastRetrievedReleaseId{};
        db::IdRange<db::ReleaseId> artistIdRange;
        while (fetchNextReleaseIdRange(session, lastRetrievedReleaseId, artistIdRange))
            queue.push(std::make_unique<ComputeReleaseArtworkAssociationsJob>(_db, searchParams, artistIdRange));

        queue.finish();

        // process all remaining associations
        updateReleasePreferredArtworks(session, artistArtworkAssociations, false);
    }
} // namespace lms::scanner
