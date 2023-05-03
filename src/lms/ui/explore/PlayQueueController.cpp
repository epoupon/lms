/*
 * Copyright (C) 2022 Emeric Poupon
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

#include "services/database/ClusterId.hpp"
#include "services/database/Release.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "explore/Filters.hpp"
#include "explore/PlayQueueController.hpp"
#include "PlayQueue.hpp"
#include "LmsApplication.hpp"

namespace UserInterface
{

	static
	std::vector<Database::TrackId>
	getArtistsTracks(Database::Session& session, const std::vector<Database::ArtistId>& artistsId, const std::vector<Database::ClusterId>& clusters, std::size_t maxTrackCount)
	{
		assert(maxTrackCount);

		std::vector<Database::TrackId> res;

		auto transaction {session.createSharedTransaction()};

		for (const Database::ArtistId artistId : artistsId)
		{
			Database::Track::FindParameters params;
			params.setArtist(artistId);
			params.setSortMethod(Database::TrackSortMethod::DateDescAndRelease);
			params.setClusters(clusters);
			params.setRange({0, maxTrackCount - res.size()});

			const auto tracks {Database::Track::find(session, params)};

			res.reserve(res.size() + tracks.results.size());
			res.insert(std::end(res), std::cbegin(tracks.results), std::cend(tracks.results));

			if (res.size() == maxTrackCount)
				break;
		}

		return res;
	}

	static
	std::vector<Database::TrackId>
	getReleasesTracks(Database::Session& session, const std::vector<Database::ReleaseId>& releasesId, const std::vector<Database::ClusterId>& clusters, std::size_t maxTrackCount)
	{
		using namespace Database;
		assert(maxTrackCount);

		std::vector<TrackId> res;

		auto transaction {session.createSharedTransaction()};

		for (const ReleaseId releaseId : releasesId)
		{
			Database::Track::FindParameters params;
			params.setRelease(releaseId);
			params.setSortMethod(Database::TrackSortMethod::Release);
			params.setClusters(clusters);
			params.setRange({0, maxTrackCount - res.size()});

			const auto tracks {Database::Track::find(session, params)};

			res.reserve(res.size() + tracks.results.size());
			res.insert(std::end(res), std::cbegin(tracks.results), std::cend(tracks.results));

			if (res.size() == maxTrackCount)
				break;
		}

		return res;
	}

	static
	std::vector<Database::TrackId>
	getTrackListTracks(Database::Session& session, Database::TrackListId trackListId, const std::vector<Database::ClusterId>& clusters, std::size_t maxTrackCount)
	{
		using namespace Database;
		assert(maxTrackCount);

		auto transaction {session.createSharedTransaction()};

		Database::Track::FindParameters params;
		params.setTrackList(trackListId);
		params.setClusters(clusters);
		params.setRange({0, maxTrackCount});
		params.setSortMethod(TrackSortMethod::TrackList);
		params.setDistinct(false);

		return Database::Track::find(session, params).results;
	}

	PlayQueueController::PlayQueueController(Filters& filters, PlayQueue& playQueue)
		: _filters {filters}
	, _playQueue {playQueue}
	{
	}

	void
	PlayQueueController::processCommand(Command command, const std::vector<Database::ArtistId>& artistIds)
	{
		const std::vector<Database::TrackId> tracks {getArtistsTracks(LmsApp->getDbSession(), artistIds, _filters.getClusterIds(), _maxTrackCountToEnqueue)};
		processCommand(command, tracks);
	}

	void
	PlayQueueController::processCommand(Command command, const std::vector<Database::ReleaseId>& releaseIds)
	{
		const std::vector<Database::TrackId> tracks {getReleasesTracks(LmsApp->getDbSession(), releaseIds, _filters.getClusterIds(), _maxTrackCountToEnqueue)};
		processCommand(command, tracks);
	}

	void
	PlayQueueController::processCommand(Command command, const std::vector<Database::TrackId>& trackIds)
	{
		// consider things are already filtered here, and _maxTrackCount honored playqueue side...
		switch (command)
		{
			case Command::Play:
				_playQueue.play(trackIds);
				break;
			case Command::PlayNext:
				_playQueue.playNext(trackIds);
				break;
			case Command::PlayShuffled:
				_playQueue.playShuffled(trackIds);
				break;
			case Command::PlayOrAddLast:
				_playQueue.playOrAddLast(trackIds);
			break;
		}
	}

	void
	PlayQueueController::processCommand(Command command, Database::TrackListId trackListId)
	{
		const std::vector<Database::TrackId> tracks {getTrackListTracks(LmsApp->getDbSession(), trackListId, _filters.getClusterIds(), _maxTrackCountToEnqueue)};
		processCommand(command, tracks);
	}

	void
	PlayQueueController::playTrackInRelease(Database::TrackId trackId)
	{
		Database::ReleaseId releaseId;
		{
			auto transaction {LmsApp->getDbSession().createSharedTransaction()};
			const Database::Track::pointer track {Database::Track::find(LmsApp->getDbSession(), trackId)};
			if (!track || !track->getRelease())
				return;

			releaseId = track->getRelease()->getId();
		}

		const std::vector<Database::TrackId> tracks {getReleasesTracks(LmsApp->getDbSession(), {releaseId}, _filters.getClusterIds(), _maxTrackCountToEnqueue)};
		auto itTrack {std::find(std::cbegin(tracks), std::cend(tracks), trackId)};
		if (itTrack == std::cend(tracks))
			return;

		const std::size_t index {static_cast<std::size_t>(std::distance(std::cbegin(tracks), itTrack))};
		_playQueue.playAtIndex(tracks, index);
	}
} // namespace UserInterface
