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

#include "database/objects/TrackBookmark.hpp"

#include <Wt/Dbo/Impl.h>

#include "database/objects/Genre.hpp"
#include "database/objects/Grouping.hpp"
#include "database/objects/Language.hpp"
#include "database/objects/Mood.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/User.hpp"

#include "Session.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::TrackBookmark)

namespace lms::db
{
    TrackBookmark::TrackBookmark(ObjectPtr<User> user, ObjectPtr<Track> track)
        : _user{ getDboPtr(user) }
        , _track{ getDboPtr(track) }
    {
    }

    TrackBookmark::pointer TrackBookmark::create(ISession& session, ObjectPtr<User> user, ObjectPtr<Track> track)
    {
        return utils::addObject(session, std::unique_ptr<TrackBookmark>{ new TrackBookmark{ user, track } });
    }

    std::size_t TrackBookmark::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM track_bookmark"));
    }

    std::vector<TrackBookmarkId> TrackBookmark::find(ISession& session, UserId userId, std::optional<Range> range)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<TrackBookmarkId>("SELECT id from track_bookmark").where("user_id = ?").bind(userId) };

        return utils::execRangeQuery<TrackBookmarkId>(query, range);
    }

    TrackBookmark::pointer TrackBookmark::find(ISession& session, UserId userId, TrackId trackId)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->find<TrackBookmark>().where("user_id = ?").bind(userId).where("track_id = ?").bind(trackId));
    }

    TrackBookmark::pointer TrackBookmark::find(ISession& session, TrackBookmarkId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->find<TrackBookmark>().where("id = ?").bind(id));
    }

} // namespace lms::db
