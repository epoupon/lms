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

#include "ScanStepComputeClusterStats.hpp"
#include "core/ILogger.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Cluster.hpp"

#include "ScanContext.hpp"

namespace lms::scanner
{
    bool ScanStepComputeClusterStats::needProcess(const ScanContext& context) const
    {
        return context.stats.getChangesCount() > 0;
    }

    void ScanStepComputeClusterStats::process(ScanContext& context)
    {
        using namespace db;

        Session& dbSession{ _db.getTLSSession() };

        const std::size_t clusterCount{ [&] {
            auto transaction{ dbSession.createReadTransaction() };
            return Cluster::getCount(dbSession);
        }() };

        context.currentStepStats.totalElems = clusterCount;

        foreachSubRange(Range{ 0, clusterCount }, 100, [&](Range range) {
            const std::vector<ClusterId> clusterIds{ [&] {
                Cluster::FindParameters params;
                params.setRange(range);

                {
                    auto transaction{ dbSession.createReadTransaction() };
                    return std::move(Cluster::findIds(dbSession, params).results);
                }
            }() };

            for (const ClusterId clusterId : clusterIds)
            {
                if (_abortScan)
                    break;

                std::size_t trackCount;
                std::size_t releaseCount;

                {
                    auto transaction{ dbSession.createReadTransaction() };

                    trackCount = Cluster::computeTrackCount(dbSession, clusterId);
                    releaseCount = Cluster::computeReleaseCount(dbSession, clusterId);
                }

                {
                    auto transaction{ dbSession.createWriteTransaction() };

                    auto cluster{ Cluster::find(dbSession, clusterId) };
                    cluster.modify()->setTrackCount(trackCount);
                    cluster.modify()->setReleaseCount(releaseCount);
                }

                context.currentStepStats.processedElems++;
                _progressCallback(context.currentStepStats);
            }

            return true;
        });

        LMS_LOG(DBUPDATER, DEBUG, "Recomputed stats for " << context.currentStepStats.processedElems << " clusters!");
    }
} // namespace lms::scanner
