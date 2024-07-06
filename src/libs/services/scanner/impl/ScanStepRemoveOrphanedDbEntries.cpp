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

#include "ScanStepRemoveOrphanedDbEntries.hpp"

#include "core/ILogger.hpp"
#include "core/Path.hpp"
#include "database/Artist.hpp"
#include "database/Cluster.hpp"
#include "database/Db.hpp"
#include "database/Directory.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"

namespace lms::scanner
{
    using namespace db;

    namespace
    {
        constexpr std::size_t batchSize = 100;

        template<typename T>
        void removeOrphanedEntries(Session& session, bool& abortScan)
        {
            using IdType = typename T::IdType;

            RangeResults<IdType> entries;
            while (!abortScan)
            {
                {
                    auto transaction{ session.createReadTransaction() };

                    entries = T::findOrphanIds(session, Range{ 0, batchSize });
                };

                {
                    auto transaction{ session.createWriteTransaction() };

                    for (const IdType objectId : entries.results)
                    {
                        if (abortScan)
                            break;

                        typename T::pointer entry{ T::find(session, objectId) };

                        entry.remove();
                    }
                }

                if (!entries.moreResults)
                    break;
            }
        }
    } // namespace

    void ScanStepRemoveOrphanedDbEntries::process(ScanContext& context)
    {
        auto& session{ _db.getTLSSession() };

        {
            auto transaction{ session.createReadTransaction() };
            context.currentStepStats.totalElems = 0;
            context.currentStepStats.totalElems += Cluster::getCount(session);
            context.currentStepStats.totalElems += ClusterType::getCount(session);
            context.currentStepStats.totalElems += Artist::getCount(session);
            context.currentStepStats.totalElems += Release::getCount(session);
            context.currentStepStats.totalElems += Directory::getCount(session);
        }
        LMS_LOG(DBUPDATER, DEBUG, context.currentStepStats.totalElems << " database entries to be checked...");

        removeOrphanedClusters();
        removeOrphanedClusterTypes();
        removeOrphanedArtists();
        removeOrphanedReleases();
        removeOrphanedDirectories();
    }

    void ScanStepRemoveOrphanedDbEntries::removeOrphanedClusters()
    {
        LMS_LOG(DBUPDATER, DEBUG, "Checking orphaned clusters...");
        removeOrphanedEntries<db::Cluster>(_db.getTLSSession(), _abortScan);
    }

    void ScanStepRemoveOrphanedDbEntries::removeOrphanedClusterTypes()
    {
        LMS_LOG(DBUPDATER, DEBUG, "Checking orphaned cluster types...");
        removeOrphanedEntries<db::ClusterType>(_db.getTLSSession(), _abortScan);
    }

    void ScanStepRemoveOrphanedDbEntries::removeOrphanedArtists()
    {
        LMS_LOG(DBUPDATER, DEBUG, "Checking orphaned artists...");
        removeOrphanedEntries<db::Artist>(_db.getTLSSession(), _abortScan);
    }

    void ScanStepRemoveOrphanedDbEntries::removeOrphanedReleases()
    {
        LMS_LOG(DBUPDATER, DEBUG, "Checking orphaned releases...");
        removeOrphanedEntries<db::Release>(_db.getTLSSession(), _abortScan);
    }

    void ScanStepRemoveOrphanedDbEntries::removeOrphanedDirectories()
    {
        LMS_LOG(DBUPDATER, DEBUG, "Checking orphaned releases...");
        removeOrphanedEntries<db::Directory>(_db.getTLSSession(), _abortScan);
    }
} // namespace lms::scanner
