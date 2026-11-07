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

#include "database/objects/RatedArtist.hpp"

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "database/objects/Artist.hpp"
#include "database/objects/User.hpp"

#include "ObjectPtrImpl.hpp"
#include "Session.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::RatedArtist)

namespace lms::db
{
    RatedArtist::RatedArtist(const ObjectPtr<Artist>& artist, const ObjectPtr<User>& user)
        : _artist{ getDboPtr(artist) }
        , _user{ getDboPtr(user) }
    {
    }

    RatedArtist::pointer RatedArtist::create(ISession& session, const ObjectPtr<Artist>& artist, const ObjectPtr<User>& user)
    {
        return utils::addObject(session, std::unique_ptr<RatedArtist>{ new RatedArtist{ artist, user } });
    }

    std::size_t RatedArtist::getCount(ISession& session)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM rated_artist"));
    }

    RatedArtist::pointer RatedArtist::find(ISession& session, RatedArtistId id)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->find<RatedArtist>().where("id = ?").bind(id));
    }

    RatedArtist::pointer RatedArtist::find(ISession& session, ArtistId artistId, UserId userId)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->find<RatedArtist>().where("artist_id = ?").bind(artistId).where("user_id = ?").bind(userId));
    }

    void RatedArtist::find(ISession& session, const FindParameters& params, std::function<void(const pointer&)> func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<RatedArtist>>("SELECT r_a FROM rated_artist r_a") };

        if (params.user.isValid())
            query.where("r_a.user_id = ?").bind(params.user);

        utils::forEachQueryRangeResult(query, params.range, func);
    }

    void RatedArtist::setLastUpdated(const Wt::WDateTime& lastUpdated)
    {
        _lastUpdated = utils::normalizeDateTime(lastUpdated);
    }
} // namespace lms::db
