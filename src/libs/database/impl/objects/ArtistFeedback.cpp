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

#include "ArtistDbo.hpp"
#include "ArtistFeedbackDbo.hpp"
#include "ObjectPtrImpl.hpp"
#include "Session.hpp"
#include "UserDbo.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::ArtistFeedback)

namespace lms::db
{
    dbo::ArtistFeedback::ArtistFeedback(const ObjectPtr<db::Artist>& artist, const ObjectPtr<db::User>& user)
        : _artist{ getDboPtr(artist) }
        , _user{ getDboPtr(user) }
    {
    }

    ArtistFeedback::pointer ArtistFeedback::create(ISession& session, const ObjectPtr<Artist>& artist, const ObjectPtr<User>& user)
    {
        return utils::addObject(session, std::unique_ptr<dbo::ArtistFeedback>{ new dbo::ArtistFeedback{ artist, user } });
    }

    std::size_t ArtistFeedback::getCount(ISession& session)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM artist_feedback"));
    }

    ArtistFeedback::pointer ArtistFeedback::find(ISession& session, ArtistFeedbackId id)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->find<dbo::ArtistFeedback>().where("id = ?").bind(id));
    }

    ArtistFeedback::pointer ArtistFeedback::find(ISession& session, ArtistId artistId, UserId userId)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->find<dbo::ArtistFeedback>().where("artist_id = ?").bind(artistId).where("user_id = ?").bind(userId));
    }

    void ArtistFeedback::setDateTime(const Wt::WDateTime& dateTime)
    {
        _dateTime = utils::normalizeDateTime(dateTime);
    }

    ObjectPtr<Artist> ArtistFeedback::getArtist() const
    {
        return getDbo(*this)._artist;
    }

    ObjectPtr<User> ArtistFeedback::getUser() const
    {
        return getDbo(*this)._user;
    }
} // namespace lms::db
