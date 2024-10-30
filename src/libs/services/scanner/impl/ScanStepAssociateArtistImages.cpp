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

#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "core/Path.hpp"
#include "database/Artist.hpp"
#include "database/Db.hpp"
#include "database/Directory.hpp"
#include "database/Image.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "image/Exception.hpp"
#include "image/Image.hpp"

namespace lms::scanner
{
    namespace
    {
        constexpr std::size_t readBatchSize{ 100 };
        constexpr std::size_t writeBatchSize{ 20 };

        struct ArtistImageAssociation
        {
            db::ArtistId artistId;
            db::ImageId imageId;
        };
        using ArtistImageAssociationContainer = std::deque<ArtistImageAssociation>;

        struct SearchImageContext
        {
            db::Session& session;
            db::ArtistId lastRetrievedArtistId;
            std::size_t processedArtistCount{};
            const std::vector<std::string>& artistFileNames;
        };

        db::Image::pointer findImageInDirectory(SearchImageContext& searchContext, const std::filesystem::path& directoryPath)
        {
            db::Image::pointer image;

            const db::Directory::pointer directory{ db::Directory::find(searchContext.session, directoryPath) };
            if (directory) // may not exist for artists that are split on different media libraries
            {
                for (std::string_view fileStem : searchContext.artistFileNames)
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

        db::Image::pointer computeBestArtistImage(SearchImageContext& searchContext, const db::Artist::pointer& artist)
        {
            db::Image::pointer image;

            const auto mbid{ artist->getMBID() };
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
                params.setArtist(artist->getId(), { db::TrackArtistLinkType::ReleaseArtist });

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
                        image = findImageInDirectory(searchContext, directoryToInspect);
                        if (image)
                            break;

                        std::filesystem::path parentPath{ directoryToInspect.parent_path() };
                        if (parentPath == directoryToInspect)
                            break;

                        directoryToInspect = parentPath;
                    }

                    if (!image)
                    {
                        // Expect layout like this:
                        // ReleaseArtist/Release/Tracks'
                        //                      /artist.jpg
                        //                      /someOtherUserConfiguredArtistFile.jpg
                        for (const std::filesystem::path& releasePath : releasePaths)
                        {
                            image = findImageInDirectory(searchContext, releasePath);
                            if (image)
                                break;
                        }
                    }
                }
            }

            return image;
        }

        bool fetchNextArtistImagesToUpdate(SearchImageContext& searchContext, ArtistImageAssociationContainer& artistImageAssociations)
        {
            const db::ArtistId artistId{ searchContext.lastRetrievedArtistId };

            {
                auto transaction{ searchContext.session.createReadTransaction() };

                db::Artist::find(searchContext.session, searchContext.lastRetrievedArtistId, readBatchSize, [&](const db::Artist::pointer& artist) {
                    db::Image::pointer image{ computeBestArtistImage(searchContext, artist) };

                    if (image != artist->getImage())
                    {
                        LMS_LOG(DBUPDATER, DEBUG, "Updating artist image for artist '" << artist->getName() << "', using '" << (image ? image->getAbsoluteFilePath().c_str() : "<none>") << "'");
                        artistImageAssociations.push_back(ArtistImageAssociation{ artist->getId(), image ? image->getId() : db::ImageId{} });
                    }
                    searchContext.processedArtistCount++;
                });
            }

            return artistId != searchContext.lastRetrievedArtistId;
        }

        void updateArtistImage(db::Session& session, const ArtistImageAssociation& artistImageAssociation)
        {
            db::Artist::pointer artist{ db::Artist::find(session, artistImageAssociation.artistId) };
            assert(artist);

            db::Image::pointer image;
            if (artistImageAssociation.imageId.isValid())
                image = db::Image::find(session, artistImageAssociation.imageId);

            artist.modify()->setImage(image);
        }

        void updateArtistImages(db::Session& session, ArtistImageAssociationContainer& imageAssociations)
        {
            while (!imageAssociations.empty())
            {
                auto transaction{ session.createWriteTransaction() };

                for (std::size_t i{}; !imageAssociations.empty() && i < writeBatchSize; ++i)
                {
                    updateArtistImage(session, imageAssociations.front());
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

    void ScanStepAssociateArtistImages::process(ScanContext& context)
    {
        if (_abortScan)
            return;

        if (context.stats.nbChanges() == 0)
            return;

        auto& session{ _db.getTLSSession() };

        {
            auto transaction{ session.createReadTransaction() };
            context.currentStepStats.totalElems = db::Artist::getCount(session);
        }

        SearchImageContext searchContext{
            .session = session,
            .lastRetrievedArtistId = {},
            .artistFileNames = _artistFileNames,
        };

        ArtistImageAssociationContainer artistImageAssociations;
        while (fetchNextArtistImagesToUpdate(searchContext, artistImageAssociations))
        {
            if (_abortScan)
                return;

            updateArtistImages(session, artistImageAssociations);
            context.currentStepStats.processedElems = searchContext.processedArtistCount;
            _progressCallback(context.currentStepStats);
        }
    }
} // namespace lms::scanner
