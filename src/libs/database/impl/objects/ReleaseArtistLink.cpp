/*
 * Copyright (C) 2025 Emeric Poupon
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

#include "database/objects/ReleaseArtistLink.hpp"

#include <Wt/Dbo/Impl.h>

#include "core/ILogger.hpp"
#include "core/String.hpp"

#include "ArtistDbo.hpp"
#include "DboAccess.hpp"
#include "ReleaseArtistLinkDbo.hpp"
#include "ReleaseDbo.hpp"
#include "Session.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::ReleaseArtistLink)

namespace lms::db
{
    ReleaseArtistLink::ReleaseArtistLink(bool artistMBIDMatched)
        : _artistMBIDMatched{ artistMBIDMatched }
    {
    }

    dbo::ReleaseArtistLink::ReleaseArtistLink(Wt::Dbo::ptr<Release> release, Wt::Dbo::ptr<Artist> artist, bool artistMBIDMatched)
        : db::ReleaseArtistLink{ artistMBIDMatched }
        , _release{ std::move(release) }
        , _artist{ std::move(artist) }
    {
    }

    ReleaseArtistLink ReleaseArtistLink::create(ISession& session, ReleaseId release, ArtistId artist, bool artistMBIDMatched)
    {
        return utils::addObject(session, std::make_unique<dbo::ReleaseArtistLink>(getDboSession(session)->loadLazy<dbo::Release>(release.getValue()), getDboSession(session)->loadLazy<dbo::Artist>(artist.getValue()), artistMBIDMatched));
    }

    void ReleaseArtistLink::destroy(ISession& session, std::span<const ReleaseArtistLinkId> ids)
    {
        utils::destroyObjects<dbo::ReleaseArtistLink>(session, ids);
    }

    std::size_t ReleaseArtistLink::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM release_artist_link"));
    }

    std::optional<ReleaseArtistLink> ReleaseArtistLink::find(ISession& session, ReleaseArtistLinkId id)
    {
        checkReadTransaction(session);

        if (!id.isValid())
            return std::nullopt;

        return utils::fetchQuerySingleObject(getDboSession(session)->query<Wt::Dbo::ptr<dbo::ReleaseArtistLink>>("SELECT r_a_l from release_artist_link r_a_l").where("r_a_l.id = ?").bind(id));
    }

    void ReleaseArtistLink::find(ISession& session, ReleaseId release, const std::function<void(const ReleaseArtistLink&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::ReleaseArtistLink>>("SELECT r_a_l from release_artist_link r_a_l").where("r_a_l.release_id = ?").bind(release) };
        utils::forEachQueryObject(query, func);
    }

    void ReleaseArtistLink::findArtistNameNoLongerMatch(ISession& session, std::optional<Range> range, const std::function<void(const ReleaseArtistLink&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::ReleaseArtistLink>>("SELECT r_a_l from release_artist_link r_a_l") };
        query.join("artist a ON r_a_l.artist_id = a.id");
        query.where("r_a_l.artist_mbid_matched = FALSE");
        query.where("r_a_l.artist_name <> a.name");

        utils::applyRange(query, range);
        utils::forEachQueryObject(query, func);
    }

    void ReleaseArtistLink::findWithArtistNameAmbiguity(ISession& session, std::optional<Range> range, bool allowArtistMBIDFallback, const std::function<void(const ReleaseArtistLink&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::ReleaseArtistLink>>("SELECT r_a_l from release_artist_link r_a_l") };
        query.join("artist a ON r_a_l.artist_id = a.id");
        query.where("r_a_l.artist_mbid_matched = FALSE");
        if (!allowArtistMBIDFallback)
        {
            query.where("a.mbid IS NOT NULL");
        }
        else
        {
            query.where(R"(
                (a.mbid IS NOT NULL AND EXISTS (SELECT 1 FROM artist a2 WHERE a2.name = a.name AND a2.mbid IS NOT NULL AND a2.mbid <> a.mbid))
                OR (a.mbid IS NULL AND (SELECT COUNT(*) FROM artist a2 WHERE a2.name = a.name AND a2.mbid IS NOT NULL) = 1))");
        }

        utils::applyRange(query, range);
        utils::forEachQueryObject(query, func);
    }

    void ReleaseArtistLink::setArtistName(std::string_view artistName)
    {
        _artistName = core::stringUtils::utf8Truncate(artistName, Artist::maxNameLength);
        LMS_LOG_IF(DB, WARNING, artistName.size() > Artist::maxNameLength, "Artist link name too long, truncated to '" << _artistName << "'");
    }

    void ReleaseArtistLink::setArtistSortName(std::string_view artistSortName)
    {
        _artistSortName = core::stringUtils::utf8Truncate(artistSortName, Artist::maxNameLength);
        LMS_LOG_IF(DB, WARNING, artistSortName.size() > Artist::maxNameLength, "Artist link sort name too long, truncated to '" << _artistSortName << "'");
    }

    void ReleaseArtistLink::update(ISession& session, const ReleaseArtistLink& releaseArtistLink)
    {
        utils::updateObject<dbo::ReleaseArtistLink>(session, releaseArtistLink);
    }
    void dbo::ReleaseArtistLink::applyIds()
    {
        setRelation(*session(), _artist, _artistId);
    }
} // namespace lms::db
