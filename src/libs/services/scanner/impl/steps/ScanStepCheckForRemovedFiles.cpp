/*
 * Copyright (C) 2023 Emeric Poupon
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

#include "ScanStepCheckForRemovedFiles.hpp"

#include <vector>

#include "core/ILogger.hpp"
#include "core/Path.hpp"
#include "database/ArtistInfo.hpp"
#include "database/IDb.hpp"
#include "database/Image.hpp"
#include "database/PlayListFile.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/TrackLyrics.hpp"

#include "ScanContext.hpp"
#include "ScannerSettings.hpp"

namespace lms::scanner
{
    bool ScanStepCheckForRemovedFiles::needProcess([[maybe_unused]] const ScanContext& context) const
    {
        // always check for removed files
        return true;
    }

    void ScanStepCheckForRemovedFiles::process(ScanContext& context)
    {
        db::Session& session{ _db.getTLSSession() };

        {
            auto transaction{ session.createReadTransaction() };
            context.currentStepStats.totalElems = 0;
            context.currentStepStats.totalElems += db::Track::getCount(session);
            context.currentStepStats.totalElems += db::Image::getCount(session);
            context.currentStepStats.totalElems += db::TrackLyrics::getExternalLyricsCount(session);
            context.currentStepStats.totalElems += db::PlayListFile::getCount(session);
            context.currentStepStats.totalElems += db::ArtistInfo::getCount(session);
        }
        LMS_LOG(DBUPDATER, DEBUG, context.currentStepStats.totalElems << " files to be checked...");

        checkForRemovedFiles<db::Track>(context);
        checkForRemovedFiles<db::Image>(context);
        checkForRemovedFiles<db::TrackLyrics>(context);
        checkForRemovedFiles<db::PlayListFile>(context);
        checkForRemovedFiles<db::ArtistInfo>(context);
    }

    template<typename Object>
    void ScanStepCheckForRemovedFiles::checkForRemovedFiles(ScanContext& context)
    {
        using namespace db;

        if (_abortScan)
            return;

        Session& session{ _db.getTLSSession() };

        std::vector<typename Object::IdType> objectIdsToRemove;

        typename Object::IdType lastCheckedId;
        bool endReached{};
        while (!endReached)
        {
            if (_abortScan)
                break;

            objectIdsToRemove.clear();
            {
                constexpr std::size_t batchSize = 200;

                auto transaction{ session.createReadTransaction() };

                endReached = true;
                Object::findAbsoluteFilePath(session, lastCheckedId, batchSize, [&](Object::IdType objectId, const std::filesystem::path& filePath) {
                    endReached = false;

                    // special case for track lyrics, only check external lyrics
                    if constexpr (std::is_same_v<Object, TrackLyrics>)
                    {
                        if (filePath.empty())
                            return;
                    }

                    if (!checkFile(filePath))
                        objectIdsToRemove.push_back(objectId);

                    context.currentStepStats.processedElems++;
                });
            }

            if (!objectIdsToRemove.empty())
            {
                auto transaction{ session.createWriteTransaction() };

                session.destroy<Object>(objectIdsToRemove);
                context.stats.deletions += objectIdsToRemove.size();
            }

            _progressCallback(context.currentStepStats);
        }
    }

    bool ScanStepCheckForRemovedFiles::checkFile(const std::filesystem::path& p)
    {
        try
        {
            // For each track, make sure the the file still exists
            // and still belongs to a media directory
            if (!std::filesystem::exists(p) || !std::filesystem::is_regular_file(p))
            {
                LMS_LOG(DBUPDATER, DEBUG, "Removing " << p << ": missing");
                return false;
            }

            if (std::none_of(std::cbegin(_settings.mediaLibraries), std::cend(_settings.mediaLibraries),
                    [&](const MediaLibraryInfo& libraryInfo) {
                        return core::pathUtils::isPathInRootPath(p, libraryInfo.rootDirectory, &excludeDirFileName);
                    }))
            {
                LMS_LOG(DBUPDATER, DEBUG, "Removing " << p << ": out of media directory");
                return false;
            }

            if (!selectFileScanner(p))
            {
                LMS_LOG(DBUPDATER, DEBUG, "Removing " << p << ": file format no longer handled");
                return false;
            }

            return true;
        }
        catch (std::filesystem::filesystem_error& e)
        {
            LMS_LOG(DBUPDATER, ERROR, "Caught exception while checking file " << p << ": " << e.what());
            return false;
        }
    }
} // namespace lms::scanner
