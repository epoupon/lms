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

#include "ScanStepAssociateArtistImages.hpp"

#include <array>
#include <cassert>
#include <deque>
#include <set>
#include <span>
#include <variant>

#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "core/Path.hpp"
#include "core/String.hpp"
#include "database/Artist.hpp"
#include "database/ArtistInfo.hpp"
#include "database/Artwork.hpp"
#include "database/Db.hpp"
#include "database/Directory.hpp"
#include "database/Image.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"

#include "ArtworkUtils.hpp"
#include "ScanContext.hpp"

namespace lms::scanner
{
    namespace
    {
        using ArtistArtwork = std::variant<std::monostate, db::ImageId>; // TODO handle embedded images in tracks?
        bool isSameArtwork(ArtistArtwork preferredArtwork, const db::ObjectPtr<db::Artwork>& artwork)
        {
            if (std::holds_alternative<std::monostate>(preferredArtwork))
                return !artwork;

            if (const db::ImageId* imageId = std::get_if<db::ImageId>(&preferredArtwork))
                return artwork && *imageId == artwork->getImageId();

            return false;
        }

        struct ArtistArtworkAssociation
        {
            db::Artist::pointer artist;
            ArtistArtwork preferredArtwork;
        };
        using ArtistArtworkAssociationContainer = std::deque<ArtistArtworkAssociation>;

        struct SearchArtistImageContext
        {
            db::Session& session;
            db::ArtistId lastRetrievedArtistId;
            std::size_t processedArtistCount{};
            std::span<const std::string> artistFileNames;
        };

        db::Image::pointer findImageInDirectory(SearchArtistImageContext& searchContext, const std::filesystem::path& directoryPath, std::span<const std::string> fileStemsToSearch)
        {
            db::Image::pointer image;

            const db::Directory::pointer directory{ db::Directory::find(searchContext.session, directoryPath) };
            if (directory) // may not exist for artists that are split on different media libraries
            {
                for (std::string_view fileStem : fileStemsToSearch)
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

        db::ImageId getImageFromMbid(SearchArtistImageContext& searchContext, const core::UUID& mbid)
        {
            db::Image::pointer image;

            // Find anywhere, since it is supposed to be unique!
            db::Image::find(searchContext.session, db::Image::FindParameters{}.setFileStem(mbid.getAsString()), [&](const db::Image::pointer foundImg) {
                if (!image)
                    image = foundImg;
            });

            return image ? image->getId() : db::ImageId{};
        }

        db::ImageId searchImageInArtistInfoDirectory(SearchArtistImageContext& searchContext, db::ArtistId artistId)
        {
            db::Image::pointer image;

            std::vector<std::string> fileInfoPaths;
            db::ArtistInfo::find(searchContext.session, artistId, [&](const db::ArtistInfo::pointer& artistInfo) {
                fileInfoPaths.push_back(artistInfo->getAbsoluteFilePath());

                if (!image)
                    image = findImageInDirectory(searchContext, artistInfo->getDirectory()->getAbsolutePath(), std::array<std::string, 2>{ "thumb", "folder" });
            });

            if (fileInfoPaths.size() > 1)
                LMS_LOG(DBUPDATER, DEBUG, "Found " << fileInfoPaths.size() << " artist info files for same artist: " << core::stringUtils::joinStrings(fileInfoPaths, ", "));

            return image ? image->getId() : db::ImageId{};
        }

        db::ImageId searchImageInDirectories(SearchArtistImageContext& searchContext, db::ArtistId artistId)
        {
            db::Image::pointer image;

            std::set<std::filesystem::path> releasePaths;
            db::Directory::FindParameters params;
            params.setArtist(artistId, { db::TrackArtistLinkType::ReleaseArtist });

            db::Directory::find(searchContext.session, params, [&](const db::Directory::pointer& directory) {
                releasePaths.insert(directory->getAbsolutePath());
            });

            if (!releasePaths.empty())
            {
                // Expect layout like this:
                // ReleaseArtist/Release/Tracks'
                //              /artist.jpg
                //              /someOtherUserConfiguredArtistFile.jpg
                //
                // Or:
                // ReleaseArtist/SomeGrouping/Release/Tracks'
                //              /artist.jpg
                //              /someOtherUserConfiguredArtistFile.jpg
                //
                std::filesystem::path directoryToInspect{ core::pathUtils::getLongestCommonPath(std::cbegin(releasePaths), std::cend(releasePaths)) };
                while (true)
                {
                    image = findImageInDirectory(searchContext, directoryToInspect, searchContext.artistFileNames);
                    if (image)
                        return image->getId();

                    std::filesystem::path parentPath{ directoryToInspect.parent_path() };
                    if (parentPath == directoryToInspect)
                        break;

                    directoryToInspect = parentPath;
                }

                // Expect layout like this:
                // ReleaseArtist/Release/Tracks'
                //                      /artist.jpg
                //                      /someOtherUserConfiguredArtistFile.jpg
                for (const std::filesystem::path& releasePath : releasePaths)
                {
                    image = findImageInDirectory(searchContext, releasePath, searchContext.artistFileNames);
                    if (image)
                        return image->getId();
                }
            }

            return image ? image->getId() : db::ImageId{};
        }

        ArtistArtwork computePreferredArtwork(SearchArtistImageContext& searchContext, const db::Artist::pointer& artist)
        {
            db::ImageId imageId;

            if (const auto mbid{ artist->getMBID() })
                imageId = getImageFromMbid(searchContext, *mbid);

            if (!imageId.isValid())
                imageId = searchImageInArtistInfoDirectory(searchContext, artist->getId());

            if (!imageId.isValid())
                imageId = searchImageInDirectories(searchContext, artist->getId());

            return imageId.isValid() ? ArtistArtwork{ imageId } : ArtistArtwork{};
        }

        bool fetchNextArtistArtworksToUpdate(SearchArtistImageContext& searchContext, ArtistArtworkAssociationContainer& ArtistArtworkAssociations)
        {
            const db::ArtistId artistId{ searchContext.lastRetrievedArtistId };

            {
                constexpr std::size_t readBatchSize{ 100 };

                auto transaction{ searchContext.session.createReadTransaction() };

                db::Artist::find(searchContext.session, searchContext.lastRetrievedArtistId, readBatchSize, [&](const db::Artist::pointer& artist) {
                    ArtistArtwork preferredArtwork{ computePreferredArtwork(searchContext, artist) };

                    if (!isSameArtwork(preferredArtwork, artist->getPreferredArtwork()))
                        ArtistArtworkAssociations.push_back(ArtistArtworkAssociation{ artist, preferredArtwork });

                    searchContext.processedArtistCount++;
                });
            }

            return artistId != searchContext.lastRetrievedArtistId;
        }

        void updateArtistPreferredArtwork(db::Session& session, const ArtistArtworkAssociation& ArtistArtworkAssociation)
        {
            db::Artist::pointer artist{ ArtistArtworkAssociation.artist };

            db::Artwork::pointer artwork;
            if (const db::ImageId * imageId{ std::get_if<db::ImageId>(&ArtistArtworkAssociation.preferredArtwork) })
                artwork = utils::getOrCreateArtworkFromImage(session, *imageId);

            artist.modify()->setPreferredArtwork(artwork);
            if (artwork)
                LMS_LOG(DBUPDATER, DEBUG, "Updated preferred artwork for artist '" << artist->getName() << "' with image in " << utils::toPath(session, artwork->getId()));
            else
                LMS_LOG(DBUPDATER, DEBUG, "Removed preferred artwork from artist '" << artist->getName() << "'");
        }

        void updateArtistArtworks(db::Session& session, ArtistArtworkAssociationContainer& imageAssociations)
        {
            constexpr std::size_t writeBatchSize{ 50 };

            while (!imageAssociations.empty())
            {
                auto transaction{ session.createWriteTransaction() };

                for (std::size_t i{}; !imageAssociations.empty() && i < writeBatchSize; ++i)
                {
                    updateArtistPreferredArtwork(session, imageAssociations.front());
                    imageAssociations.pop_front();
                }
            }
        }

        std::vector<std::string> constructArtistFileNames()
        {
            std::vector<std::string> res;

            core::Service<core::IConfig>::get()->visitStrings("artist-image-file-names",
                [&res](std::string_view fileName) {
                    res.emplace_back(fileName);
                },
                { "artist" });

            return res;
        }

    } // namespace

    ScanStepAssociateArtistImages::ScanStepAssociateArtistImages(InitParams& initParams)
        : ScanStepBase{ initParams }
        , _artistFileNames{ constructArtistFileNames() }
    {
    }

    bool ScanStepAssociateArtistImages::needProcess(const ScanContext& context) const
    {
        return context.stats.nbChanges() > 0;
    }

    void ScanStepAssociateArtistImages::process(ScanContext& context)
    {
        auto& session{ _db.getTLSSession() };

        {
            auto transaction{ session.createReadTransaction() };
            context.currentStepStats.totalElems = db::Artist::getCount(session);
        }

        SearchArtistImageContext searchContext{
            .session = session,
            .lastRetrievedArtistId = {},
            .artistFileNames = _artistFileNames,
        };

        ArtistArtworkAssociationContainer ArtistArtworkAssociations;
        while (fetchNextArtistArtworksToUpdate(searchContext, ArtistArtworkAssociations))
        {
            if (_abortScan)
                return;

            updateArtistArtworks(session, ArtistArtworkAssociations);
            context.currentStepStats.processedElems = searchContext.processedArtistCount;
            _progressCallback(context.currentStepStats);
        }
    }
} // namespace lms::scanner
