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
#include "core/ILogger.hpp"
#include "core/Path.hpp"

namespace lms::scanner
{
    using namespace db;

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
        using namespace db;

        if (_abortScan)
            return;

        Session& session{ _db.getTLSSession() };

        LMS_LOG(DBUPDATER, DEBUG, "Checking tracks to be removed...");
        {
            auto transaction{ session.createReadTransaction() };
            context.currentStepStats.totalElems = Track::getCount(session);
        }
        LMS_LOG(DBUPDATER, DEBUG, context.currentStepStats.totalElems << " tracks to be checked...");

        // TODO handle only files in context.directory?
        std::vector<Track::pointer> tracksToRemove;

        TrackId lastCheckedTrackID;
        bool endReached{};
        while (!endReached)
        {
            if (_abortScan)
                break;

            tracksToRemove.clear();
            {
                auto transaction{ session.createReadTransaction() };

                endReached = true;
                Track::find(session, lastCheckedTrackID, batchSize, [&](const Track::pointer& track)
                    {
                        endReached = false;

                        if (!checkFile(track->getAbsoluteFilePath()))
                            tracksToRemove.push_back(track);

                        context.currentStepStats.processedElems++;
                    });
            }

            if (!tracksToRemove.empty())
            {
                auto transaction{ session.createWriteTransaction() };

                for (Track::pointer& track : tracksToRemove)
                {
                    track.remove();
                    context.stats.deletions++;
                }
            }

            _progressCallback(context.currentStepStats);
        }

        LMS_LOG(DBUPDATER, DEBUG, context.currentStepStats.processedElems << " tracks checked!");
    }

    void ScanStepRemoveOrphanDbFiles::removeOrphanClusters()
    {
        LMS_LOG(DBUPDATER, DEBUG, "Checking orphan clusters...");
        removeOrphanEntries<db::Cluster>(_db.getTLSSession(), _abortScan);
    }

    void ScanStepRemoveOrphanDbFiles::removeOrphanClusterTypes()
    {
        LMS_LOG(DBUPDATER, DEBUG, "Checking orphan cluster types...");
        removeOrphanEntries<db::ClusterType>(_db.getTLSSession(), _abortScan);
    }

    void ScanStepRemoveOrphanDbFiles::removeOrphanArtists()
    {
        LMS_LOG(DBUPDATER, DEBUG, "Checking orphan artists...");
        removeOrphanEntries<db::Artist>(_db.getTLSSession(), _abortScan);
    }

    void ScanStepRemoveOrphanDbFiles::removeOrphanReleases()
    {
        LMS_LOG(DBUPDATER, DEBUG, "Checking orphan releases...");
        removeOrphanEntries<db::Release>(_db.getTLSSession(), _abortScan);
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
                    return core::pathUtils::isPathInRootPath(p, libraryInfo.rootDirectory, &excludeDirFileName);
                }))
            {
                LMS_LOG(DBUPDATER, INFO, "Removing '" << p.string() << "': out of media directory");
                return false;
            }

            if (!core::pathUtils::hasFileAnyExtension(p, _settings.supportedExtensions))
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
