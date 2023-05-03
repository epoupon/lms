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

#include "services/database/TrackId.hpp"
#include "services/database/Object.hpp"
#include "services/database/StarredTrackId.hpp"
#include "services/database/Types.hpp"
#include "services/database/UserId.hpp"
#include "utils/EnumSet.hpp"

namespace Database
{
	class Track;
	class Session;
	class User;

	class StarredTrack final : public Object<StarredTrack, StarredTrackId>
	{
		public:
			StarredTrack() = default;

			struct FindParameters
			{
				std::optional<Scrobbler>    	scrobbler;      	// for this scrobbler
				std::optional<ScrobblingState>	scrobblingState;	//   and these states
				UserId							user;				// and this user
				Range							range;

				FindParameters& setScrobbler(Scrobbler _scrobbler, ScrobblingState _scrobblingState) { scrobbler = _scrobbler; scrobblingState = _scrobblingState; return *this; }
				FindParameters& setUser(UserId _user) {user = _user; return *this; }
				FindParameters& setRange(Range _range) {range = _range; return *this; }
			};

			// Search utility
			static std::size_t	getCount(Session& session);
			static pointer		find(Session& session, StarredTrackId id);
			static pointer		find(Session& session, TrackId trackId, UserId userId, Scrobbler scrobbler);
			static RangeResults<StarredTrackId>	find(Session& session, const FindParameters& findParams);

			// Accessors
			ObjectPtr<Track>	getTrack() const { return _track; }
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

				Wt::Dbo::belongsTo(a,	_track,		"track",	Wt::Dbo::OnDeleteCascade);
				Wt::Dbo::belongsTo(a,	_user,		"user",		Wt::Dbo::OnDeleteCascade);
			}

		private:
			friend class Session;
			StarredTrack(ObjectPtr<Track> track, ObjectPtr<User> user, Scrobbler scrobbler);
			static pointer create(Session& session, ObjectPtr<Track> track, ObjectPtr<User> user, Scrobbler scrobbler);

			Scrobbler		_scrobbler;			// for which scrobbler
			ScrobblingState	_scrobblingState {ScrobblingState::PendingAdd};
			Wt::WDateTime	_dateTime;			// when it was starred

			Wt::Dbo::ptr<Track>		_track;
			Wt::Dbo::ptr<User>		_user;
	};
} // namespace Database

