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
#include "core/ILogger.hpp"
#include "core/Path.hpp"
#include "database/Db.hpp"
#include "database/Directory.hpp"
#include "database/Image.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"

#include "ScanContext.hpp"

namespace lms::scanner
{
    namespace
    {
        struct ReleaseImageAssociation
        {
            db::ReleaseId releaseId;
            db::ImageId imageId;
        };
        using ReleaseImageAssociationContainer = std::deque<ReleaseImageAssociation>;

        struct SearchReleaseImageContext
        {
            db::Session& session;
            db::ReleaseId lastRetrievedReleaseId;
            std::size_t processedReleaseCount{};
            const std::vector<std::string>& releaseFileNames;
        };

        db::Image::pointer findImageInDirectory(SearchReleaseImageContext& searchContext, const std::filesystem::path& directoryPath)
        {
            db::Image::pointer image;

            const db::Directory::pointer directory{ db::Directory::find(searchContext.session, directoryPath) };
            if (directory) // may not exist for releases that are split on different media libraries
            {
                for (std::string_view fileStem : searchContext.releaseFileNames)
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

        bool fetchNextReleaseImagesToUpdate(SearchReleaseImageContext& searchContext, ReleaseImageAssociationContainer& releaseImageAssociations)
        {
            const db::ReleaseId releaseId{ searchContext.lastRetrievedReleaseId };

            {
                constexpr std::size_t readBatchSize{ 100 };

                auto transaction{ searchContext.session.createReadTransaction() };

                db::Release::find(searchContext.session, searchContext.lastRetrievedReleaseId, readBatchSize, [&](const db::Release::pointer& release) {
                    db::Image::pointer image{ computeBestReleaseImage(searchContext, release) };

                    if (image != release->getImage())
                    {
                        LMS_LOG(DBUPDATER, DEBUG, "Updating release image for release '" << release->getName() << "', using '" << (image ? image->getAbsoluteFilePath().c_str() : "<none>") << "'");
                        releaseImageAssociations.push_back(ReleaseImageAssociation{ release->getId(), image ? image->getId() : db::ImageId{} });
                    }
                    searchContext.processedReleaseCount++;
                });
            }

            return releaseId != searchContext.lastRetrievedReleaseId;
        }

        void updateReleaseImage(db::Session& session, const ReleaseImageAssociation& releaseImageAssociation)
        {
            db::Release::pointer release{ db::Release::find(session, releaseImageAssociation.releaseId) };
            assert(release);

            db::Image::pointer image;
            if (releaseImageAssociation.imageId.isValid())
                image = db::Image::find(session, releaseImageAssociation.imageId);

            release.modify()->setImage(image);
        }

        void updateReleaseImages(db::Session& session, ReleaseImageAssociationContainer& imageAssociations)
        {
            constexpr std::size_t writeBatchSize{ 20 };

            while (!imageAssociations.empty())
            {
                auto transaction{ session.createWriteTransaction() };

                for (std::size_t i{}; !imageAssociations.empty() && i < writeBatchSize; ++i)
                {
                    updateReleaseImage(session, imageAssociations.front());
                    imageAssociations.pop_front();
                }
            }
        }

        std::vector<std::string> constructReleaseFileNames()
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
        , _releaseFileNames{ constructReleaseFileNames() }
    {
    }

    bool ScanStepAssociateReleaseImages::needProcess(const ScanContext& context) const
    {
        if (context.stats.nbChanges() > 0)
            return true;

        return false;
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
            .releaseFileNames = _releaseFileNames,
        };

        ReleaseImageAssociationContainer releaseImageAssociations;
        while (fetchNextReleaseImagesToUpdate(searchContext, releaseImageAssociations))
        {
            if (_abortScan)
                return;

            updateReleaseImages(session, releaseImageAssociations);
            context.currentStepStats.processedElems = searchContext.processedReleaseCount;
            _progressCallback(context.currentStepStats);
        }
    }
} // namespace lms::scanner
