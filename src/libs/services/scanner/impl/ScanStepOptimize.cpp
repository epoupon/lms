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

#include "ScanStepOptimize.hpp"

#include "core/ILogger.hpp"
#include "database/Db.hpp"
#include "database/Session.hpp"

namespace lms::scanner
{
    void ScanStepOptimize::process(ScanContext& context)
    {
        ScanStats& stats{ context.stats };

        if (context.scanOptions.forceOptimize || (stats.nbChanges() > (stats.nbFiles() / 5)))
        {
            LMS_LOG(DBUPDATER, INFO, "Database analyze started");

            auto& session{ _db.getTLSSession() };

            std::vector<std::string> entries;
            session.retrieveEntriesToAnalyze(entries);
            context.currentStepStats.totalElems = entries.size();
            _progressCallback(context.currentStepStats);

            for (const std::string& entry : entries)
            {
                if (_abortScan)
                    break;

                _db.getTLSSession().analyzeEntry(entry);
                context.currentStepStats.processedElems++;
                _progressCallback(context.currentStepStats);
            }

            LMS_LOG(DBUPDATER, INFO, "Database analyze complete");
        }
    }
} // namespace lms::scanner
