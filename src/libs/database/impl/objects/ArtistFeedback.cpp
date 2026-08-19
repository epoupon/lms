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

#include "database/objects/ArtistFeedback.hpp"

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/User.hpp"

#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::ArtistFeedback)

namespace lms::db
{
    ArtistFeedback::ArtistFeedback(ObjectPtr<Artist> artist, ObjectPtr<User> user)
        : _artist{ getDboPtr(artist) }
        , _user{ getDboPtr(user) }
    {
    }

    ArtistFeedback::pointer ArtistFeedback::create(Session& session, ObjectPtr<Artist> artist, ObjectPtr<User> user)
    {
        return session.getDboSession()->add(std::unique_ptr<ArtistFeedback>{ new ArtistFeedback{ artist, user } });
    }

    std::size_t ArtistFeedback::getCount(Session& session)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM artist_feedback"));
    }

    ArtistFeedback::pointer ArtistFeedback::find(Session& session, ArtistFeedbackId id)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->find<ArtistFeedback>().where("id = ?").bind(id));
    }

    ArtistFeedback::pointer ArtistFeedback::find(Session& session, ArtistId artistId, UserId userId)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->find<ArtistFeedback>().where("artist_id = ?").bind(artistId).where("user_id = ?").bind(userId));
    }

    void ArtistFeedback::setDateTime(const Wt::WDateTime& dateTime)
    {
        _dateTime = utils::normalizeDateTime(dateTime);
    }
} // namespace lms::db
