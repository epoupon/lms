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
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackEmbeddedImage.hpp"

#include "ScanContext.hpp"

namespace lms::scanner
{
    bool ScanStepRemoveOrphanedDbEntries::needProcess([[maybe_unused]] const ScanContext& context) const
    {
        // fast enough when there is nothing to do
        return true;
    }

    void ScanStepRemoveOrphanedDbEntries::process(ScanContext& context)
    {
        removeOrphanedClusters(context);
        removeOrphanedClusterTypes(context);
        removeOrphanedArtists(context);
        removeOrphanedReleases(context);
        removeOrphanedReleaseTypes(context);
        removeOrphanedLabels(context);
        removeOrphanedCountries(context);
        removeOrphanedDirectories(context);
        removeOrphanedTrackEmbeddedImages(context);
    }

    void ScanStepRemoveOrphanedDbEntries::removeOrphanedClusters(ScanContext& context)
    {
        LMS_LOG(DBUPDATER, DEBUG, "Checking orphaned clusters...");
        removeOrphanedEntries<db::Cluster>(context);
    }

    void ScanStepRemoveOrphanedDbEntries::removeOrphanedClusterTypes(ScanContext& context)
    {
        LMS_LOG(DBUPDATER, DEBUG, "Checking orphaned cluster types...");
        removeOrphanedEntries<db::ClusterType>(context);
    }

    void ScanStepRemoveOrphanedDbEntries::removeOrphanedArtists(ScanContext& context)
    {
        LMS_LOG(DBUPDATER, DEBUG, "Checking orphaned artists...");
        removeOrphanedEntries<db::Artist>(context);
    }

    void ScanStepRemoveOrphanedDbEntries::removeOrphanedReleases(ScanContext& context)
    {
        LMS_LOG(DBUPDATER, DEBUG, "Checking orphaned releases...");
        removeOrphanedEntries<db::Release>(context);
    }

    void ScanStepRemoveOrphanedDbEntries::removeOrphanedReleaseTypes(ScanContext& context)
    {
        LMS_LOG(DBUPDATER, DEBUG, "Checking orphaned release types...");
        removeOrphanedEntries<db::ReleaseType>(context);
    }

    void ScanStepRemoveOrphanedDbEntries::removeOrphanedLabels(ScanContext& context)
    {
        LMS_LOG(DBUPDATER, DEBUG, "Checking orphaned labels...");
        removeOrphanedEntries<db::Label>(context);
    }

    void ScanStepRemoveOrphanedDbEntries::removeOrphanedCountries(ScanContext& context)
    {
        LMS_LOG(DBUPDATER, DEBUG, "Checking orphaned countries...");
        removeOrphanedEntries<db::Country>(context);
    }

    void ScanStepRemoveOrphanedDbEntries::removeOrphanedDirectories(ScanContext& context)
    {
        LMS_LOG(DBUPDATER, DEBUG, "Checking orphaned directories...");
        removeOrphanedEntries<db::Directory>(context);
    }

    void ScanStepRemoveOrphanedDbEntries::removeOrphanedTrackEmbeddedImages(ScanContext& context)
    {
        LMS_LOG(DBUPDATER, DEBUG, "Checking orphaned embedded images in tracks...");
        removeOrphanedEntries<db::TrackEmbeddedImage>(context);
    }

    template<typename T>
    void ScanStepRemoveOrphanedDbEntries::removeOrphanedEntries(ScanContext& context)
    {
        constexpr std::size_t batchSize = 200;

        using IdType = typename T::IdType;

        db::Session& session{ _db.getTLSSession() };

        db::RangeResults<IdType> entries;
        while (!_abortScan)
        {
            {
                auto transaction{ session.createReadTransaction() };

                entries = T::findOrphanIds(session, db::Range{ 0, batchSize });
            };

            if (entries.results.empty())
                break;

            {
                auto transaction{ session.createWriteTransaction() };

                session.destroy<T>(entries.results);
            }

            context.currentStepStats.processedElems += entries.results.size();
            _progressCallback(context.currentStepStats);
        }
    }
} // namespace lms::scanner
