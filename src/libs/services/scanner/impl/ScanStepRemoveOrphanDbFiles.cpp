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

#include "services/database/Artist.hpp"
#include "services/database/Cluster.hpp"
#include "services/database/Db.hpp"
#include "services/database/Release.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "utils/Logger.hpp"
#include "utils/Path.hpp"

namespace Scanner
{
	void
	ScanStepRemoveOrphanDbFiles::process(ScanContext& context)
	{
		removeOrphanTracks(context);
		removeOrphanClusters();
		removeOrphanArtists();
		removeOrphanReleases();
	}

	void ScanStepRemoveOrphanDbFiles::removeOrphanTracks(ScanContext& context)
	{
		using namespace Database;

		if (_abortScan)
			return;

		static constexpr std::size_t batchSize {50};
		Session& session {_db.getTLSSession()};

		LMS_LOG(DBUPDATER, DEBUG) << "Checking tracks to be removed...";
		std::size_t trackCount {};

		{
			auto transaction {session.createSharedTransaction()};
			trackCount = Track::getCount(session);
		}
		LMS_LOG(DBUPDATER, DEBUG) << trackCount << " tracks to be checked...";

		context.currentStepStats.totalElems = trackCount;

		RangeResults<Track::PathResult> trackPaths;
		std::vector<TrackId> tracksToRemove;

		// TODO handle only files in context.directory
		for (std::size_t i {trackCount < batchSize ? 0 : trackCount - batchSize}; ; i -= (i > batchSize ? batchSize : i))
		{
			tracksToRemove.clear();

			{
				auto transaction {session.createSharedTransaction()};
				trackPaths = Track::findPaths(session, Range {i, batchSize});
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
				auto transaction {session.createSharedTransaction()};

				for (const TrackId trackId : tracksToRemove)
				{
					Track::pointer track {Track::find(session, trackId)};
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

		LMS_LOG(DBUPDATER, DEBUG) << trackCount << " tracks checked!";
	}

	void
	ScanStepRemoveOrphanDbFiles::removeOrphanClusters()
	{
		using namespace Database;

		LMS_LOG(DBUPDATER, DEBUG) << "Checking orphan clusters...";
		Session& session {_db.getTLSSession()};
		auto transaction {session.createUniqueTransaction()};

		// Now process orphan Cluster (no track)
		auto clusterIds {Cluster::findOrphans(session, Range {})};
		for (ClusterId clusterId : clusterIds.results)
		{
			Cluster::pointer cluster {Cluster::find(session, clusterId)};
			LMS_LOG(DBUPDATER, DEBUG) << "Removing orphan cluster '" << cluster->getName() << "'";
			cluster.remove();
		}
	}

	void
	ScanStepRemoveOrphanDbFiles::removeOrphanArtists()
	{
		using namespace Database;

		LMS_LOG(DBUPDATER, DEBUG) << "Checking orphan artists...";

		Session& session {_db.getTLSSession()};
		auto transaction {session.createUniqueTransaction()};

		auto artistIds {Artist::findAllOrphans(session, Range {})};
		for (const ArtistId artistId : artistIds.results)
		{
			Artist::pointer artist {Artist::find(session, artistId)};
			LMS_LOG(DBUPDATER, DEBUG) << "Removing orphan artist '" << artist->getName() << "'";
			artist.remove();
		}
	}

	void
	ScanStepRemoveOrphanDbFiles::removeOrphanReleases()
	{
		using namespace Database;

		LMS_LOG(DBUPDATER, DEBUG) << "Checking orphan releases...";

		Session& session {_db.getTLSSession()};
		auto transaction {session.createUniqueTransaction()};

		auto releases {Release::findOrphans(session, Range {})};
		for (const ReleaseId releaseId : releases.results)
		{
			Release::pointer release {Release::find(session, releaseId)};
			LMS_LOG(DBUPDATER, DEBUG) << "Removing orphan release '" << release->getName() << "'";
			release.remove();
		}
	}

	bool
	ScanStepRemoveOrphanDbFiles::checkFile(const std::filesystem::path& p)
	{
		try
		{
			// For each track, make sure the the file still exists
			// and still belongs to a media directory
			if (!std::filesystem::exists( p )
					|| !std::filesystem::is_regular_file( p ) )
			{
				LMS_LOG(DBUPDATER, INFO) << "Removing '" << p.string() << "': missing";
				return false;
			}

			if (!PathUtils::isPathInRootPath(p, _settings.mediaDirectory, &excludeDirFileName))
			{
				LMS_LOG(DBUPDATER, INFO) << "Removing '" << p.string() << "': out of media directory";
				return false;
			}

			if (!PathUtils::hasFileAnyExtension(p, _settings.supportedExtensions))
			{
				LMS_LOG(DBUPDATER, INFO) << "Removing '" << p.string() << "': file format no longer handled";
				return false;
			}

			return true;
		}
		catch (std::filesystem::filesystem_error& e)
		{
			LMS_LOG(DBUPDATER, ERROR) << "Caught exception while checking file '" << p.string() << "': " << e.what();
			return false;
		}
	}
}
