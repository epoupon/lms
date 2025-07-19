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

#include "ScanStepUpdateLibraryFields.hpp"

#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/MediaLibrary.hpp"

#include "MediaLibraryInfo.hpp"
#include "ScanContext.hpp"
#include "ScannerSettings.hpp"

namespace lms::scanner
{
    bool ScanStepUpdateLibraryFields::needProcess([[maybe_unused]] const ScanContext& context) const
    {
        // Fast enough when nothing to do
        return true;
    }

    void ScanStepUpdateLibraryFields::process(ScanContext& context)
    {
        processDirectories(context);
    }

    void ScanStepUpdateLibraryFields::processDirectories(ScanContext& context)
    {
        for (const MediaLibraryInfo& mediaLibrary : _settings.mediaLibraries)
        {
            if (_abortScan)
                break;

            processDirectory(context, mediaLibrary);
        }
    }

    void ScanStepUpdateLibraryFields::processDirectory(ScanContext& context, const MediaLibraryInfo& mediaLibrary)
    {
        db::Session& session{ _db.getTLSSession() };

        constexpr std::size_t batchSize = 100;

        db::RangeResults<db::DirectoryId> entries;
        while (!_abortScan)
        {
            {
                auto transaction{ session.createReadTransaction() };

                entries = db::Directory::findMismatchedLibrary(session, db::Range{ 0, batchSize }, mediaLibrary.rootDirectory, mediaLibrary.id);
            };

            if (entries.results.empty())
                break;

            {
                auto transaction{ session.createWriteTransaction() };

                db::MediaLibrary::pointer library{ db::MediaLibrary::find(session, mediaLibrary.id) };
                if (!library) // may be legit
                    break;

                for (const db::DirectoryId directoryId : entries.results)
                {
                    if (_abortScan)
                        break;

                    db::Directory::pointer directory{ db::Directory::find(session, directoryId) };
                    directory.modify()->setMediaLibrary(library);
                }
            }

            context.currentStepStats.processedElems += entries.results.size();
            _progressCallback(context.currentStepStats);
        }
    }
} // namespace lms::scanner
