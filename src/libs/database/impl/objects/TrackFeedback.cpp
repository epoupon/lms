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

#include "database/objects/TrackFeedback.hpp"

#include <cassert>

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "DboAccess.hpp"
#include "Session.hpp"
#include "TrackDbo.hpp"
#include "TrackFeedbackDbo.hpp"
#include "UserDbo.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::TrackFeedback)

namespace lms::db
{
    dbo::TrackFeedback::TrackFeedback(Wt::Dbo::ptr<Track> track, Wt::Dbo::ptr<User> user)
        : _track{ std::move(track) }
        , _user{ std::move(user) }
    {
    }

    TrackFeedback TrackFeedback::create(ISession& session, TrackId track, UserId user)
    {
        return utils::addObject(session, std::unique_ptr<dbo::TrackFeedback>{ new dbo::TrackFeedback{ getDboSession(session)->loadLazy<dbo::Track>(track.getValue()), getDboSession(session)->loadLazy<dbo::User>(user.getValue()) } });
    }

    void TrackFeedback::destroy(ISession& session, std::span<const TrackFeedbackId> ids)
    {
        utils::destroyObjects<dbo::TrackFeedback>(session, ids);
    }

    std::size_t TrackFeedback::getCount(ISession& session)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM track_feedback"));
    }

    std::optional<TrackFeedback> TrackFeedback::find(ISession& session, TrackFeedbackId id)
    {
        checkReadTransaction(session);

        if (!id.isValid())
            return std::nullopt;

        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::TrackFeedback>().where("id = ?").bind(id));
    }

    std::optional<TrackFeedback> TrackFeedback::find(ISession& session, TrackId trackId, UserId userId)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::TrackFeedback>().where("track_id = ?").bind(trackId).where("user_id = ?").bind(userId));
    }

    void TrackFeedback::find(ISession& session, const FindParameters& params, const std::function<void(const TrackFeedback&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->find<dbo::TrackFeedback>() };

        if (params.user.isValid())
            query.where("user_id = ?").bind(params.user);

        if (params.lastRetrievedId.isValid())
        {
            assert(params.sortMethod == TrackFeedbackSortMethod::Id); // keyset pagination only makes sense with a stable id order
            query.where("id > ?").bind(params.lastRetrievedId);
        }

        switch (params.sortMethod)
        {
        case TrackFeedbackSortMethod::None:
            break;
        case TrackFeedbackSortMethod::Id:
            query.orderBy("id");
            break;
        }

        utils::forEachQueryRangeObject(query, params.range, func);
    }

    void TrackFeedback::setDateTime(const Wt::WDateTime& dateTime)
    {
        _dateTime = utils::normalizeDateTime(dateTime);
    }

    void TrackFeedback::update(ISession& session, const TrackFeedback& trackFeedback)
    {
        utils::updateObject<dbo::TrackFeedback>(session, trackFeedback);
    }
} // namespace lms::db
