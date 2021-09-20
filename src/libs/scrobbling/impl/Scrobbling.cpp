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

#include "Scrobbling.hpp"

#include "database/Db.hpp"
#include "database/Session.hpp"
#include "database/TrackList.hpp"
#include "database/User.hpp"

#include "internal/InternalScrobbler.hpp"
#include "listenbrainz/ListenBrainzScrobbler.hpp"

namespace Scrobbling
{
	std::unique_ptr<IScrobbling>
	createScrobbling(boost::asio::io_context& ioContext, Database::Db& db)
	{
		return std::make_unique<Scrobbling>(ioContext, db);
	}

	Scrobbling::Scrobbling(boost::asio::io_context& ioContext, Database::Db& db)
		: _db {db}
	{
		_scrobblers.emplace(Database::Scrobbler::Internal, std::make_unique<InternalScrobbler>(_db));
		_scrobblers.emplace(Database::Scrobbler::ListenBrainz, std::make_unique<ListenBrainz::Scrobbler>(ioContext, _db));
	}

	void
	Scrobbling::listenStarted(const Listen& listen)
	{
		if (std::optional<Database::Scrobbler> scrobbler {getUserScrobbler(listen.userId)})
			_scrobblers[*scrobbler]->listenStarted(listen);
	}

	void
	Scrobbling::listenFinished(const Listen& listen, std::optional<std::chrono::seconds> duration)
	{
		if (std::optional<Database::Scrobbler> scrobbler {getUserScrobbler(listen.userId)})
			_scrobblers[*scrobbler]->listenFinished(listen, duration);
	}

	void
	Scrobbling::addTimedListen(const TimedListen& listen)
	{
		if (std::optional<Database::Scrobbler> scrobbler {getUserScrobbler(listen.userId)})
			_scrobblers[*scrobbler]->addTimedListen(listen);
	}

	std::optional<Database::Scrobbler>
	Scrobbling::getUserScrobbler(Database::UserId userId)
	{
		std::optional<Database::Scrobbler> scrobbler;

		Database::Session& session {_db.getTLSSession()};
		auto transaction {session.createSharedTransaction()};
		if (const Database::User::pointer user {Database::User::getById(session, userId)})
			scrobbler = user->getScrobbler();

		return scrobbler;
	}

	std::vector<Database::ObjectPtr<Database::Artist>>
	Scrobbling::getRecentArtists(Database::Session& session,
										Database::ObjectPtr<Database::User> user,
										const std::vector<Database::ClusterId>& clusterIds,
										std::optional<Database::TrackArtistLinkType> linkType,
										std::optional<Database::Range> range,
										bool& moreResults)
	{
		const Database::ObjectPtr<Database::TrackList> history {getListensTrackList(session, user)};

		std::vector<Database::ObjectPtr<Database::Artist>> res;
		if (history)
			res = history->getArtistsReverse(clusterIds, linkType, range, moreResults);

		return res;
	}

	std::vector<Database::ObjectPtr<Database::Release>>
	Scrobbling::getRecentReleases(Database::Session& session,
										Database::ObjectPtr<Database::User> user,
										const std::vector<Database::ClusterId>& clusterIds,
										std::optional<Database::Range> range,
										bool& moreResults)
	{
		const Database::ObjectPtr<Database::TrackList> history {getListensTrackList(session, user)};

		std::vector<Database::ObjectPtr<Database::Release>> res;
		if (history)
			res = history->getReleasesReverse(clusterIds, range, moreResults);

		return res;
	}

	std::vector<Database::ObjectPtr<Database::Track>>
	Scrobbling::getRecentTracks(Database::Session& session,
										Database::ObjectPtr<Database::User> user,
										const std::vector<Database::ClusterId>& clusterIds,
										std::optional<Database::Range> range,
										bool& moreResults)
	{
		const Database::ObjectPtr<Database::TrackList> history {getListensTrackList(session, user)};

		std::vector<Database::ObjectPtr<Database::Track>> res;
		if (history)
			res = history->getTracksReverse(clusterIds, range, moreResults);

		return res;
	}


	// Top
	std::vector<Database::ObjectPtr<Database::Artist>>
	Scrobbling::getTopArtists(Database::Session& session,
										Database::ObjectPtr<Database::User> user,
										const std::vector<Database::ClusterId>& clusterIds,
										std::optional<Database::TrackArtistLinkType> linkType,
										std::optional<Database::Range> range,
										bool& moreResults)
	{
		const Database::ObjectPtr<Database::TrackList> history {getListensTrackList(session, user)};

		std::vector<Database::ObjectPtr<Database::Artist>> res;
		if (history)
			res = history->getTopArtists(clusterIds, linkType, range, moreResults);

		return res;
	}

	std::vector<Database::ObjectPtr<Database::Release>>
	Scrobbling::getTopReleases(Database::Session& session,
										Database::ObjectPtr<Database::User> user,
										const std::vector<Database::ClusterId>& clusterIds,
										std::optional<Database::Range> range,
										bool& moreResults)
	{
		const Database::ObjectPtr<Database::TrackList> history {getListensTrackList(session, user)};

		std::vector<Database::ObjectPtr<Database::Release>> res;
		if (history)
			res = history->getTopReleases(clusterIds, range, moreResults);

		return res;
	}

	std::vector<Database::ObjectPtr<Database::Track>>
	Scrobbling::getTopTracks(Database::Session& session,
										Database::ObjectPtr<Database::User> user,
										const std::vector<Database::ClusterId>& clusterIds,
										std::optional<Database::Range> range,
										bool& moreResults)
	{
		const Database::ObjectPtr<Database::TrackList> history {getListensTrackList(session, user)};

		std::vector<Database::ObjectPtr<Database::Track>> res;
		if (history)
			res = history->getTopTracks(clusterIds, range, moreResults);

		return res;
	}

	Database::ObjectPtr<Database::TrackList>
	Scrobbling::getListensTrackList(Database::Session& session, Database::ObjectPtr<Database::User> user)
	{
		return _scrobblers[user->getScrobbler()]->getListensTrackList(session, user);
	}

} // ns Scrobbling

