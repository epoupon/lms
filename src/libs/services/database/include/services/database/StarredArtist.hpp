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

#pragma once

#include <Wt/WDateTime.h>
#include <Wt/Dbo/Dbo.h>

#include "services/database/ArtistId.hpp"
#include "services/database/Object.hpp"
#include "services/database/StarredArtistId.hpp"
#include "services/database/Types.hpp"
#include "services/database/UserId.hpp"

namespace Database
{
	class Artist;
	class Session;
	class User;

	class StarredArtist final : public Object<StarredArtist, StarredArtistId>
	{
		public:
			StarredArtist() = default;

			// Search utility
			static std::size_t	getCount(Session& session);
			static pointer		find(Session& session, StarredArtistId id);
			static pointer		find(Session& session, ArtistId artistId, UserId userId, Scrobbler scrobbler);

			// Accessors
			ObjectPtr<Artist>	getArtist() const { return _artist; }
			ObjectPtr<User>		getUser() const { return _user; }
			Scrobbler			getScrobbler() const { return _scrobbler; }
			const Wt::WDateTime& getDateTime() const { return _dateTime; }
			ScrobblingState		getScrobblingState() const { return _scrobblingState; }

			// Setters
			void setDateTime(const Wt::WDateTime& dateTime);
			void setScrobblingState(ScrobblingState state) { _scrobblingState  = state; }

			template<class Action>
			void persist(Action& a)
			{
				Wt::Dbo::field(a,	_scrobbler,			"scrobbler");
				Wt::Dbo::field(a,	_scrobblingState,	"scrobbling_state");
				Wt::Dbo::field(a,	_dateTime,			"date_time");

				Wt::Dbo::belongsTo(a,	_artist,	"artist",	Wt::Dbo::OnDeleteCascade);
				Wt::Dbo::belongsTo(a,	_user,		"user",		Wt::Dbo::OnDeleteCascade);
			}

		private:
			friend class Session;
			StarredArtist(ObjectPtr<Artist> artist, ObjectPtr<User> user, Scrobbler scrobbler);
			static pointer create(Session& session, ObjectPtr<Artist> artist, ObjectPtr<User> user, Scrobbler scrobbler);

			Scrobbler		_scrobbler;			// for which scrobbler
			ScrobblingState	_scrobblingState {ScrobblingState::PendingAdd};
			Wt::WDateTime	_dateTime;			// when it was starred

			Wt::Dbo::ptr<Artist>	_artist;
			Wt::Dbo::ptr<User>		_user;
	};
} // namespace Database

