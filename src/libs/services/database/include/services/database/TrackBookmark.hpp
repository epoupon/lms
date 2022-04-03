/*
 * Copyright (C) 2020 Emeric Poupon
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

#include <chrono>

#include <Wt/Dbo/Dbo.h>

#include "services/database/IdType.hpp"
#include "services/database/Object.hpp"
#include "services/database/TrackId.hpp"
#include "services/database/Types.hpp"
#include "services/database/UserId.hpp"

LMS_DECLARE_IDTYPE(TrackBookmarkId)

namespace Database {

class Session;
class Track;
class User;

class TrackBookmark : public Object<TrackBookmark, TrackBookmarkId>
{
	public:
		TrackBookmark () = default;
		TrackBookmark(ObjectPtr<User> user, ObjectPtr<Track> track);

		// utility
		static pointer create(Session& session, ObjectPtr<User> user, ObjectPtr<Track> track);

		// Find utility functions
		static std::size_t						getCount(Session& session);
		static pointer							find(Session& session, TrackBookmarkId id);
		static RangeResults<TrackBookmarkId>	find(Session& session, UserId userId, Range range);
		static pointer							find(Session& session, UserId userId, TrackId trackId);

		// Setters
		void setOffset(std::chrono::milliseconds offset)	{ _offset = offset; }
		void setComment(std::string_view comment)		{ _comment = comment; }

		// Getters
		std::chrono::milliseconds	getOffset() const	{ return _offset; }
		std::string_view			getComment() const	{ return _comment; }
		ObjectPtr<Track>			getTrack() const	{ return _track; }
		ObjectPtr<User>				getUser() const		{ return _user; }

		template<class Action>
			void persist(Action& a)
			{
				Wt::Dbo::field(a, _offset,		"offset");
				Wt::Dbo::field(a, _comment,		"comment");
				Wt::Dbo::belongsTo(a, _track, 		"track", Wt::Dbo::OnDeleteCascade);
				Wt::Dbo::belongsTo(a, _user, 		"user", Wt::Dbo::OnDeleteCascade);
			}
	private:
		static const std::size_t _maxCommentLength = 128;

		std::chrono::duration<int, std::milli>	_offset;
		std::string				_comment;

		Wt::Dbo::ptr<User>			_user;
		Wt::Dbo::ptr<Track>			_track;
};

} // namespace database


