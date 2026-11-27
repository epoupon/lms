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
#include "DboAccess.hpp"
#include "Session.hpp"
#include "UserDbo.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::ArtistFeedback)

namespace lms::db
{
    dbo::ArtistFeedback::ArtistFeedback(Wt::Dbo::ptr<Artist> artist, Wt::Dbo::ptr<User> user)
        : _artist{ std::move(artist) }
        , _user{ std::move(user) }
    {
    }

    ArtistFeedback ArtistFeedback::create(ISession& session, ArtistId artist, UserId user)
    {
        return utils::addObject(session, std::unique_ptr<dbo::ArtistFeedback>{ new dbo::ArtistFeedback{ getDboSession(session)->loadLazy<dbo::Artist>(artist.getValue()), getDboSession(session)->loadLazy<dbo::User>(user.getValue()) } });
    }

    void ArtistFeedback::destroy(ISession& session, std::span<const ArtistFeedbackId> ids)
    {
        utils::destroyObjects<dbo::ArtistFeedback>(session, ids);
    }

    std::size_t ArtistFeedback::getCount(ISession& session)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM artist_feedback"));
    }

    std::optional<ArtistFeedback> ArtistFeedback::find(ISession& session, ArtistFeedbackId id)
    {
        checkReadTransaction(session);

        if (!id.isValid())
            return std::nullopt;

        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::ArtistFeedback>().where("id = ?").bind(id));
    }

    std::optional<ArtistFeedback> ArtistFeedback::find(ISession& session, ArtistId artistId, UserId userId)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::ArtistFeedback>().where("artist_id = ?").bind(artistId).where("user_id = ?").bind(userId));
    }

    void ArtistFeedback::setDateTime(const Wt::WDateTime& dateTime)
    {
        _dateTime = utils::normalizeDateTime(dateTime);
    }

    void ArtistFeedback::update(ISession& session, const ArtistFeedback& artistFeedback)
    {
        utils::updateObject<dbo::ArtistFeedback>(session, artistFeedback);
    }
} // namespace lms::db
