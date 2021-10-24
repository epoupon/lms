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

#include "FeedbackService.hpp"
#include "services/database/Db.hpp"
#include "services/database/Artist.hpp"
#include "services/database/Release.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "services/database/User.hpp"

namespace
{
	using namespace Database;

	void
	exec(Db& db, UserId userId, ArtistId artistId, std::function<void(User::pointer, Artist::pointer)> func)
	{
		Session& dbSession {db.getTLSSession()};
		auto transaction {dbSession.createSharedTransaction()};

		Database::User::pointer user {Database::User::getById(dbSession, userId)};
		if (!user)
			return;

		Database::Artist::pointer artist {Database::Artist::getById(dbSession, artistId)};
		if (!artist)
			return;

		func(user, artist);
	}

	void
	exec(Db& db, UserId userId, ReleaseId releaseId, std::function<void(User::pointer, Release::pointer)> func)
	{
		Session& dbSession {db.getTLSSession()};
		auto transaction {dbSession.createSharedTransaction()};

		Database::User::pointer user {Database::User::getById(dbSession, userId)};
		if (!user)
			return;

		Database::Release::pointer release {Database::Release::getById(dbSession, releaseId)};
		if (!release)
			return;

		func(user, release);
	}

	void
	exec(Db& db, UserId userId, TrackId trackId, std::function<void(User::pointer, Track::pointer)> func)
	{
		Session& dbSession {db.getTLSSession()};
		auto transaction {dbSession.createSharedTransaction()};

		Database::User::pointer user {Database::User::getById(dbSession, userId)};
		if (!user)
			return;

		Database::Track::pointer track {Database::Track::getById(dbSession, trackId)};
		if (!track)
			return;

		func(user, track);
	}
}

namespace Feedback
{
	std::unique_ptr<IFeedbackService>
	createFeedbackService(Database::Db& db)
	{
		return std::make_unique<FeedbackService>(db);
	}

	FeedbackService::FeedbackService(Database::Db& db)
	: _db {db}
	{
	}

	void
	FeedbackService::star(Database::UserId userId, Database::ArtistId artistId)
	{
		starObject(userId, artistId);
	}

	void
	FeedbackService::unstar(Database::UserId userId, Database::ArtistId artistId)
	{
		unstarObject(userId, artistId);
	}

	bool
	FeedbackService::isStarred(Database::UserId userId, Database::ArtistId artistId)
	{
		return isObjectStarred(userId, artistId);
	}

	void
	FeedbackService::star(Database::UserId userId, Database::ReleaseId releaseId)
	{
		starObject(userId, releaseId);
	}

	void
	FeedbackService::unstar(Database::UserId userId, Database::ReleaseId releaseId)
	{
		unstarObject(userId, releaseId);
	}

	bool
	FeedbackService::isStarred(Database::UserId userId, Database::ReleaseId releaseId)
	{
		return isObjectStarred(userId, releaseId);
	}

	void
	FeedbackService::star(Database::UserId userId, Database::TrackId trackId)
	{
		starObject(userId, trackId);
	}

	void
	FeedbackService::unstar(Database::UserId userId, Database::TrackId trackId)
	{
		unstarObject(userId, trackId);
	}

	bool
	FeedbackService::isStarred(Database::UserId userId, Database::TrackId trackId)
	{
		return isObjectStarred(userId, trackId);
	}

	template <typename DatabaseId>
	void
	FeedbackService::starObject(Database::UserId userId, DatabaseId objectId)
	{
		exec(_db, userId, objectId, [](Database::User::pointer user, auto object)
		{
			user.modify()->star(object);
		});
	}

	template <typename DatabaseId>
	void
	FeedbackService::unstarObject(Database::UserId userId, DatabaseId objectId)
	{
		exec(_db, userId, objectId, [](Database::User::pointer user, auto object)
		{
			user.modify()->unstar(object);
		});
	}

	template <typename DatabaseId>
	bool
	FeedbackService::isObjectStarred(Database::UserId userId, DatabaseId objectId)
	{
		bool res {};
		exec(_db, userId, objectId, [&res](Database::User::pointer user, auto object)
		{
			res = user->isStarred(object);
		});
		return res;
	}
} // ns Feedback

