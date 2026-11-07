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

#include "database/IDb.hpp"

#include "ScanContext.hpp"

namespace lms::scanner
{
    bool ScanStepOptimize::needProcess(const ScanContext& context) const
    {
        if (context.scanOptions.forceOptimize)
            return true;

        // Don't optimize if there are too few files: it may lead to some indexes not being used
        // and will drastically slow down the scan process when adding more files later
        if (context.stats.getChangesCount() > (context.stats.getTotalFileCount() / 5)
            && context.stats.getTotalFileCount() >= 1'000)
        {
            return true;
        }

        return false;
    }

    void ScanStepOptimize::process(ScanContext& context)
    {
        _db.analyze(
            [&](std::size_t processedEntries, std::size_t totalEntries) {
                context.currentStepStats.processedElems = processedEntries;
                context.currentStepStats.totalElems = totalEntries;
                _progressCallback(context.currentStepStats);
            },
            [this] { return _abortScan; });
    }
} // namespace lms::scanner
