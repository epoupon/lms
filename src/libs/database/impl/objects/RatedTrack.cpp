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

#include "database/objects/RatedTrack.hpp"

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "DboAccess.hpp"
#include "RatedTrackDbo.hpp"
#include "Session.hpp"
#include "TrackDbo.hpp"
#include "UserDbo.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::RatedTrack)

namespace lms::db
{
    dbo::RatedTrack::RatedTrack(Wt::Dbo::ptr<Track> track, Wt::Dbo::ptr<User> user)
        : _track{ std::move(track) }
        , _user{ std::move(user) }
    {
    }

    RatedTrack RatedTrack::create(ISession& session, TrackId track, UserId user)
    {
        return utils::addObject(session, std::unique_ptr<dbo::RatedTrack>{ new dbo::RatedTrack{ getDboSession(session)->loadLazy<dbo::Track>(track.getValue()), getDboSession(session)->loadLazy<dbo::User>(user.getValue()) } });
    }

    void RatedTrack::destroy(ISession& session, std::span<const RatedTrackId> ids)
    {
        utils::destroyObjects<dbo::RatedTrack>(session, ids);
    }

    std::size_t RatedTrack::getCount(ISession& session)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM rated_track"));
    }

    std::optional<RatedTrack> RatedTrack::find(ISession& session, RatedTrackId id)
    {
        checkReadTransaction(session);

        if (!id.isValid())
            return std::nullopt;

        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::RatedTrack>().where("id = ?").bind(id));
    }

    std::optional<RatedTrack> RatedTrack::find(ISession& session, TrackId trackId, UserId userId)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::RatedTrack>().where("track_id = ?").bind(trackId).where("user_id = ?").bind(userId));
    }

    void RatedTrack::find(ISession& session, const FindParameters& params, const std::function<void(const RatedTrack&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::RatedTrack>>("SELECT r_t FROM rated_track r_t") };

        if (params.user.isValid())
            query.where("r_t.user_id = ?").bind(params.user);

        utils::forEachQueryRangeObject(query, params.range, func);
    }

    void RatedTrack::setLastUpdated(const Wt::WDateTime& lastUpdated)
    {
        _lastUpdated = utils::normalizeDateTime(lastUpdated);
    }

    void RatedTrack::update(ISession& session, const RatedTrack& ratedTrack)
    {
        utils::updateObject<dbo::RatedTrack>(session, ratedTrack);
    }
} // namespace lms::db
