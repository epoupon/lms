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

#include "DboAccess.hpp"
#include "Session.hpp"
#include "TrackBookmarkDbo.hpp"
#include "TrackDbo.hpp"
#include "UserDbo.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::TrackBookmark)

namespace lms::db
{
    dbo::TrackBookmark::TrackBookmark(Wt::Dbo::ptr<User> user, Wt::Dbo::ptr<Track> track)
        : _user{ std::move(user) }
        , _track{ std::move(track) }
    {
    }

    TrackBookmark TrackBookmark::create(ISession& session, UserId user, TrackId track)
    {
        return utils::addObject(session, std::unique_ptr<dbo::TrackBookmark>{ new dbo::TrackBookmark{ getDboSession(session)->loadLazy<dbo::User>(user.getValue()), getDboSession(session)->loadLazy<dbo::Track>(track.getValue()) } });
    }

    void TrackBookmark::destroy(ISession& session, std::span<const TrackBookmarkId> ids)
    {
        utils::destroyObjects<dbo::TrackBookmark>(session, ids);
    }

    std::size_t TrackBookmark::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM track_bookmark"));
    }

    void TrackBookmark::find(ISession& session, UserId userId, const std::function<void(const TrackBookmark&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->find<dbo::TrackBookmark>().where("user_id = ?").bind(userId) };

        utils::forEachQueryObject(query, func);
    }

    std::optional<TrackBookmark> TrackBookmark::find(ISession& session, UserId userId, TrackId trackId)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::TrackBookmark>().where("user_id = ?").bind(userId).where("track_id = ?").bind(trackId));
    }

    std::optional<TrackBookmark> TrackBookmark::find(ISession& session, TrackBookmarkId id)
    {
        checkReadTransaction(session);

        if (!id.isValid())
            return std::nullopt;

        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::TrackBookmark>().where("id = ?").bind(id));
    }

    void TrackBookmark::update(ISession& session, const TrackBookmark& trackBookmark)
    {
        utils::updateObject<dbo::TrackBookmark>(session, trackBookmark);
    }
} // namespace lms::db
