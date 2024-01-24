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

#include "ScanStepRemoveOrphanDbFiles.hpp"

#include "database/Artist.hpp"
#include "database/Cluster.hpp"
#include "database/Db.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "utils/ILogger.hpp"
#include "utils/Path.hpp"

namespace Scanner
{
    using namespace Database;

    namespace
    {
        constexpr std::size_t batchSize = 100;

        template <typename T>
        void removeOrphanEntries(Session& session, bool& abortScan)
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
    }

    void ScanStepRemoveOrphanDbFiles::process(ScanContext& context)
    {
        removeOrphanTracks(context);
        removeOrphanClusters();
        removeOrphanClusterTypes();
        removeOrphanArtists();
        removeOrphanReleases();
    }

    void ScanStepRemoveOrphanDbFiles::removeOrphanTracks(ScanContext& context)
    {
        using namespace Database;

        if (_abortScan)
            return;

        Session& session{ _db.getTLSSession() };

        LMS_LOG(DBUPDATER, DEBUG, "Checking tracks to be removed...");
        std::size_t trackCount{};

        {
            auto transaction{ session.createReadTransaction() };
            trackCount = Track::getCount(session);
        }
        LMS_LOG(DBUPDATER, DEBUG,  trackCount << " tracks to be checked...");

        context.currentStepStats.totalElems = trackCount;

        RangeResults<Track::PathResult> trackPaths;
        std::vector<TrackId> tracksToRemove;

        // TODO handle only files in context.directory
        for (std::size_t i{ trackCount < batchSize ? 0 : trackCount - batchSize }; ; i -= (i > batchSize ? batchSize : i))
        {
            tracksToRemove.clear();

            {
                auto transaction{ session.createReadTransaction() };
                trackPaths = Track::findPaths(session, Range{ i, batchSize });
            }

            for (const Track::PathResult& trackPath : trackPaths.results)
            {
                if (_abortScan)
                    return;

                if (!checkFile(trackPath.path))
                    tracksToRemove.push_back(trackPath.trackId);

                context.currentStepStats.processedElems++;
            }

            if (!tracksToRemove.empty())
            {
                auto transaction{ session.createWriteTransaction() };

                for (const TrackId trackId : tracksToRemove)
                {
                    Track::pointer track{ Track::find(session, trackId) };
                    if (track)
                    {
                        track.remove();
                        context.stats.deletions++;
                    }
                }
            }

            _progressCallback(context.currentStepStats);

            if (i == 0)
                break;
        }

        LMS_LOG(DBUPDATER, DEBUG,  trackCount << " tracks checked!");
    }

    void ScanStepRemoveOrphanDbFiles::removeOrphanClusters()
    {
        LMS_LOG(DBUPDATER, DEBUG, "Checking orphan clusters...");
        removeOrphanEntries<Database::Cluster>(_db.getTLSSession(), _abortScan);
    }

    void ScanStepRemoveOrphanDbFiles::removeOrphanClusterTypes()
    {
        LMS_LOG(DBUPDATER, DEBUG, "Checking orphan cluster types...");
        removeOrphanEntries<Database::ClusterType>(_db.getTLSSession(), _abortScan);
    }

    void ScanStepRemoveOrphanDbFiles::removeOrphanArtists()
    {
        LMS_LOG(DBUPDATER, DEBUG, "Checking orphan artists...");
        removeOrphanEntries<Database::Artist>(_db.getTLSSession(), _abortScan);
    }

    void ScanStepRemoveOrphanDbFiles::removeOrphanReleases()
    {
        LMS_LOG(DBUPDATER, DEBUG, "Checking orphan releases...");
        removeOrphanEntries<Database::Release>(_db.getTLSSession(), _abortScan);
    }

    bool ScanStepRemoveOrphanDbFiles::checkFile(const std::filesystem::path& p)
    {
        try
        {
            // For each track, make sure the the file still exists
            // and still belongs to a media directory
            if (!std::filesystem::exists(p) || !std::filesystem::is_regular_file(p))
            {
                LMS_LOG(DBUPDATER, INFO, "Removing '" << p.string() << "': missing");
                return false;
            }

            if (std::none_of(std::cbegin(_settings.mediaLibraries), std::cend(_settings.mediaLibraries),
                [&](const ScannerSettings::MediaLibraryInfo& libraryInfo)
                {
                    return PathUtils::isPathInRootPath(p, libraryInfo.rootDirectory, &excludeDirFileName);
                }))
            {
                LMS_LOG(DBUPDATER, INFO, "Removing '" << p.string() << "': out of media directory");
                return false;
            }

            if (!PathUtils::hasFileAnyExtension(p, _settings.supportedExtensions))
            {
                LMS_LOG(DBUPDATER, INFO, "Removing '" << p.string() << "': file format no longer handled");
                return false;
            }

            return true;
        }
        catch (std::filesystem::filesystem_error& e)
        {
            LMS_LOG(DBUPDATER, ERROR, "Caught exception while checking file '" << p.string() << "': " << e.what());
            return false;
        }
    }
}
