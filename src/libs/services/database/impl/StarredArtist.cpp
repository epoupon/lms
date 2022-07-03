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

#include "services/database/StarredArtist.hpp"

#include <Wt/Dbo/WtSqlTraits.h>

#include "services/database/Artist.hpp"
#include "services/database/Session.hpp"
#include "services/database/User.hpp"
#include "IdTypeTraits.hpp"
#include "Utils.hpp"

namespace Database
{
	StarredArtist::StarredArtist(ObjectPtr<Artist> artist, ObjectPtr<User> user, Scrobbler scrobbler)
		: _scrobbler {scrobbler}
		, _artist {getDboPtr(artist)}
		, _user {getDboPtr(user)}
	{
	}

	StarredArtist::pointer
	StarredArtist::create(Session& session, ObjectPtr<Artist> artist, ObjectPtr<User> user, Scrobbler scrobbler)
	{
		return session.getDboSession().add(std::unique_ptr<StarredArtist> {new StarredArtist {artist, user, scrobbler}});
	}

	std::size_t
	StarredArtist::getCount(Session& session)
	{
		session.checkSharedLocked();
		return session.getDboSession().query<int>("SELECT COUNT(*) FROM starred_artist");
	}

	StarredArtist::pointer
	StarredArtist::find(Session& session, StarredArtistId id)
	{
		session.checkSharedLocked();
		return session.getDboSession().find<StarredArtist>().where("id = ?").bind(id).resultValue();
	}

	StarredArtist::pointer
	StarredArtist::find(Session& session, ArtistId artistId, UserId userId, Scrobbler scrobbler)
	{
		session.checkSharedLocked();
		return session.getDboSession().find<StarredArtist>()
			.where("artist_id = ?").bind(artistId)
			.where("user_id = ?").bind(userId)
			.where("scrobbler = ?").bind(scrobbler)
			.resultValue();
	}

	void
	StarredArtist::setDateTime(const Wt::WDateTime& dateTime)
	{
		_dateTime = Utils::normalizeDateTime(dateTime);
	}
}
