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

#include "database/Session.hpp"
#include "database/objects/Genre.hpp"
#include "database/objects/Grouping.hpp"
#include "database/objects/Language.hpp"
#include "database/objects/Mood.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/User.hpp"

#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::TrackFeedback)

namespace lms::db
{
    TrackFeedback::TrackFeedback(ObjectPtr<Track> track, ObjectPtr<User> user)
        : _track{ getDboPtr(track) }
        , _user{ getDboPtr(user) }
    {
    }

    TrackFeedback::pointer TrackFeedback::create(Session& session, ObjectPtr<Track> track, ObjectPtr<User> user)
    {
        return session.getDboSession()->add(std::unique_ptr<TrackFeedback>{ new TrackFeedback{ track, user } });
    }

    std::size_t TrackFeedback::getCount(Session& session)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM track_feedback"));
    }

    TrackFeedback::pointer TrackFeedback::find(Session& session, TrackFeedbackId id)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->find<TrackFeedback>().where("id = ?").bind(id));
    }

    TrackFeedback::pointer TrackFeedback::find(Session& session, TrackId trackId, UserId userId)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->find<TrackFeedback>().where("track_id = ?").bind(trackId).where("user_id = ?").bind(userId));
    }

    void TrackFeedback::find(Session& session, const FindParameters& params, const std::function<void(const pointer&)>& func)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->find<TrackFeedback>() };

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

        utils::forEachQueryRangeResult(query, params.range, func);
    }

    void TrackFeedback::setDateTime(const Wt::WDateTime& dateTime)
    {
        _dateTime = utils::normalizeDateTime(dateTime);
    }
} // namespace lms::db
