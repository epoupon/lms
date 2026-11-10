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

#include "ArtistDbo.hpp"
#include "DboAccess.hpp"
#include "RatedArtistDbo.hpp"
#include "Session.hpp"
#include "UserDbo.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::RatedArtist)

namespace lms::db
{
    dbo::RatedArtist::RatedArtist(Wt::Dbo::ptr<Artist> artist, Wt::Dbo::ptr<User> user)
        : _artist{ std::move(artist) }
        , _user{ std::move(user) }
    {
    }

    RatedArtist RatedArtist::create(ISession& session, ArtistId artist, UserId user)
    {
        return utils::addObject(session, std::unique_ptr<dbo::RatedArtist>{ new dbo::RatedArtist{ getDboSession(session)->loadLazy<dbo::Artist>(artist.getValue()), getDboSession(session)->loadLazy<dbo::User>(user.getValue()) } });
    }

    void RatedArtist::destroy(ISession& session, std::span<const RatedArtistId> ids)
    {
        utils::destroyObjects<dbo::RatedArtist>(session, ids);
    }

    std::size_t RatedArtist::getCount(ISession& session)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM rated_artist"));
    }

    std::optional<RatedArtist> RatedArtist::find(ISession& session, RatedArtistId id)
    {
        checkReadTransaction(session);

        if (!id.isValid())
            return std::nullopt;

        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::RatedArtist>().where("id = ?").bind(id));
    }

    std::optional<RatedArtist> RatedArtist::find(ISession& session, ArtistId artistId, UserId userId)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::RatedArtist>().where("artist_id = ?").bind(artistId).where("user_id = ?").bind(userId));
    }

    void RatedArtist::find(ISession& session, const FindParameters& params, const std::function<void(const RatedArtist&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::RatedArtist>>("SELECT r_a FROM rated_artist r_a") };

        if (params.user.isValid())
            query.where("r_a.user_id = ?").bind(params.user);

        utils::forEachQueryRangeObject(query, params.range, func);
    }

    void RatedArtist::setLastUpdated(const Wt::WDateTime& lastUpdated)
    {
        _lastUpdated = utils::normalizeDateTime(lastUpdated);
    }

    void RatedArtist::update(ISession& session, const RatedArtist& ratedArtist)
    {
        utils::updateObject<dbo::RatedArtist>(session, ratedArtist);
    }
} // namespace lms::db
