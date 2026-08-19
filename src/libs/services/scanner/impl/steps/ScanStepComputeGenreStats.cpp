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

#include "ScanStepComputeGenreStats.hpp"

#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Genre.hpp"

#include "ScanContext.hpp"

namespace lms::scanner
{
    bool ScanStepComputeGenreStats::needProcess(const ScanContext& context) const
    {
        return context.stats.getChangesCount() > 0;
    }

    void ScanStepComputeGenreStats::process(ScanContext& context)
    {
        using namespace db;

        Session& dbSession{ _db.getTLSSession() };

        const std::size_t genreCount{ [&] {
            auto transaction{ dbSession.createReadTransaction() };
            return Genre::getCount(dbSession);
        }() };

        context.currentStepStats.totalElems = genreCount;

        foreachSubRange(Range{ 0, genreCount }, 100, [&](Range range) {
            const std::vector<GenreId> genreIds{ [&] {
                Genre::FindParameters params;
                params.setRange(range);
                auto transaction{ dbSession.createReadTransaction() };
                return Genre::findIds(dbSession, params);
            }() };

            for (const GenreId genreId : genreIds)
            {
                if (_abortScan)
                    break;

                std::size_t trackCount;
                std::size_t releaseCount;
                {
                    auto transaction{ dbSession.createReadTransaction() };
                    trackCount = Genre::computeTrackCount(dbSession, genreId);
                    releaseCount = Genre::computeReleaseCount(dbSession, genreId);
                }

                {
                    auto transaction{ dbSession.createWriteTransaction() };
                    auto genre{ Genre::find(dbSession, genreId) };
                    genre.modify()->setTrackCount(trackCount);
                    genre.modify()->setReleaseCount(releaseCount);
                }

                context.currentStepStats.processedElems++;
                _progressCallback(context.currentStepStats);
            }

            return true;
        });
    }
} // namespace lms::scanner
