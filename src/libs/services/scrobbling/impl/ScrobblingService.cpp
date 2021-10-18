/*
 * Copyright (C) 2021 Emeric Poupon
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

#include "ScrobblingService.hpp"

#include "database/Artist.hpp"
#include "database/Db.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/TrackList.hpp"
#include "database/User.hpp"

#include "internal/InternalScrobbler.hpp"
#include "listenbrainz/ListenBrainzScrobbler.hpp"

namespace Scrobbling
{
	using namespace Database;

	std::unique_ptr<IScrobblingService>
	createScrobblingService(boost::asio::io_context& ioContext, Db& db)
	{
		return std::make_unique<ScrobblingService>(ioContext, db);
	}

	ScrobblingService::ScrobblingService(boost::asio::io_context& ioContext, Db& db)
		: _db {db}
	{
		_scrobblers.emplace(Database::Scrobbler::Internal, std::make_unique<InternalScrobbler>(_db));
		_scrobblers.emplace(Database::Scrobbler::ListenBrainz, std::make_unique<ListenBrainz::Scrobbler>(ioContext, _db));
	}

	void
	ScrobblingService::listenStarted(const Listen& listen)
	{
		if (std::optional<Database::Scrobbler> scrobbler {getUserScrobbler(listen.userId)})
			_scrobblers[*scrobbler]->listenStarted(listen);
	}

	void
	ScrobblingService::listenFinished(const Listen& listen, std::optional<std::chrono::seconds> duration)
	{
		if (std::optional<Database::Scrobbler> scrobbler {getUserScrobbler(listen.userId)})
			_scrobblers[*scrobbler]->listenFinished(listen, duration);
	}

	void
	ScrobblingService::addTimedListen(const TimedListen& listen)
	{
		if (std::optional<Database::Scrobbler> scrobbler {getUserScrobbler(listen.userId)})
			_scrobblers[*scrobbler]->addTimedListen(listen);
	}

	std::optional<Database::Scrobbler>
	ScrobblingService::getUserScrobbler(Database::UserId userId)
	{
		std::optional<Database::Scrobbler> scrobbler;

		Session& session {_db.getTLSSession()};
		auto transaction {session.createSharedTransaction()};
		if (const User::pointer user {User::getById(session, userId)})
			scrobbler = user->getScrobbler();

		return scrobbler;
	}

	ScrobblingService::ArtistContainer
	ScrobblingService::getRecentArtists(UserId userId,
									const std::vector<ClusterId>& clusterIds,
									std::optional<TrackArtistLinkType> linkType,
									std::optional<Range> range,
									bool& moreResults)
	{
		ArtistContainer res;

		Session& session {_db.getTLSSession()};
		auto transaction {session.createSharedTransaction()};

		const User::pointer user {User::getById(session, userId)};
		if (!user)
			return res;

		const ObjectPtr<TrackList> history {getListensTrackList(session, user)};
		if (history)
		{
			for (const Artist::pointer& artist : history->getArtistsReverse(clusterIds, linkType, range, moreResults))
				res.push_back(artist->getId());
		}

		return res;
	}

	ScrobblingService::ReleaseContainer
	ScrobblingService::getRecentReleases(Database::UserId userId,
									const std::vector<Database::ClusterId>& clusterIds,
									std::optional<Database::Range> range,
									bool& moreResults)
	{
		ReleaseContainer res;

		Session& session {_db.getTLSSession()};
		auto transaction {session.createSharedTransaction()};

		const User::pointer user {User::getById(session, userId)};
		if (!user)
			return res;

		const ObjectPtr<TrackList> history {getListensTrackList(session, user)};
		if (history)
		{
			for (const Release::pointer& release : history->getReleasesReverse(clusterIds, range, moreResults))
				res.push_back(release->getId());
		}

		return res;
	}

	ScrobblingService::TrackContainer
	ScrobblingService::getRecentTracks(Database::UserId userId,
										const std::vector<Database::ClusterId>& clusterIds,
										std::optional<Database::Range> range,
										bool& moreResults)
	{
		TrackContainer res;

		Session& session {_db.getTLSSession()};
		auto transaction {session.createSharedTransaction()};

		const User::pointer user {User::getById(session, userId)};
		if (!user)
			return res;

		const ObjectPtr<TrackList> history {getListensTrackList(session, user)};
		if (history)
		{
			for (const Track::pointer& track : history->getTracksReverse(clusterIds, range, moreResults))
				res.push_back(track->getId());
		}

		return res;
	}


	// Top
	ScrobblingService::ArtistContainer
	ScrobblingService::getTopArtists(UserId userId,
								const std::vector<Database::ClusterId>& clusterIds,
								std::optional<Database::TrackArtistLinkType> linkType,
								std::optional<Database::Range> range,
								bool& moreResults)
	{
		ArtistContainer res;

		Session& session {_db.getTLSSession()};
		auto transaction {session.createSharedTransaction()};

		const User::pointer user {User::getById(session, userId)};
		if (!user)
			return res;

		const ObjectPtr<TrackList> history {getListensTrackList(session, user)};
		if (history)
		{
			for (const Artist::pointer& artist : history->getTopArtists(clusterIds, linkType, range, moreResults))
				res.push_back(artist->getId());
		}

		return res;
	}

	ScrobblingService::ReleaseContainer
	ScrobblingService::getTopReleases(Database::UserId userId,
								const std::vector<Database::ClusterId>& clusterIds,
								std::optional<Database::Range> range,
								bool& moreResults)
	{
		ReleaseContainer res;

		Session& session {_db.getTLSSession()};
		auto transaction {session.createSharedTransaction()};

		const User::pointer user {User::getById(session, userId)};
		if (!user)
			return res;

		const ObjectPtr<TrackList> history {getListensTrackList(session, user)};
		if (history)
		{
			for (const Release::pointer& release : history->getTopReleases(clusterIds, range, moreResults))
				res.push_back(release->getId());
		}

		return res;
	}

	ScrobblingService::TrackContainer
	ScrobblingService::getTopTracks(Database::UserId userId,
								const std::vector<Database::ClusterId>& clusterIds,
								std::optional<Database::Range> range,
								bool& moreResults)
	{
		TrackContainer res;

		Session& session {_db.getTLSSession()};
		auto transaction {session.createSharedTransaction()};

		const User::pointer user {User::getById(session, userId)};
		if (!user)
			return res;

		if (const ObjectPtr<TrackList> history {getListensTrackList(session, user)})
		{
			for (const Track::pointer& track : history->getTopTracks(clusterIds, range, moreResults))
				res.push_back(track->getId());
		}

		return res;
	}

	Database::ObjectPtr<Database::TrackList>
	ScrobblingService::getListensTrackList(Session& session, Database::ObjectPtr<Database::User> user)
	{
		return _scrobblers[user->getScrobbler()]->getListensTrackList(session, user);
	}

} // ns Scrobbling

