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
#include "ObjectPtrImpl.hpp"
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

    dbo::ReleaseArtistLink::ReleaseArtistLink(const ObjectPtr<db::Release>& release, const ObjectPtr<db::Artist>& artist, bool artistMBIDMatched)
        : db::ReleaseArtistLink{ artistMBIDMatched }
        , _release{ getDboPtr(release) }
        , _artist{ getDboPtr(artist) }
    {
    }

    ReleaseArtistLink::pointer ReleaseArtistLink::create(ISession& session, const ObjectPtr<Release>& release, const ObjectPtr<Artist>& artist, bool artistMBIDMatched)
    {
        return utils::addObject(session, std::make_unique<dbo::ReleaseArtistLink>(release, artist, artistMBIDMatched));
    }

    std::size_t ReleaseArtistLink::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM track_artist_link"));
    }

    ReleaseArtistLink::pointer ReleaseArtistLink::find(ISession& session, ReleaseArtistLinkId id)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<Wt::Dbo::ptr<dbo::ReleaseArtistLink>>("SELECT r_a_l from release_artist_link r_a_l").where("r_a_l.id = ?").bind(id));
    }

    void ReleaseArtistLink::findArtistNameNoLongerMatch(ISession& session, std::optional<Range> range, const std::function<void(const ReleaseArtistLink::pointer&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::ReleaseArtistLink>>("SELECT r_a_l from release_artist_link r_a_l") };
        query.join("artist a ON r_a_l.artist_id = a.id");
        query.where("r_a_l.artist_mbid_matched = FALSE");
        query.where("r_a_l.artist_name <> a.name");

        utils::applyRange(query, range);
        utils::forEachQueryResult(query, [&](const ReleaseArtistLink::pointer& link) {
            func(link);
        });
    }

    void ReleaseArtistLink::findWithArtistNameAmbiguity(ISession& session, std::optional<Range> range, bool allowArtistMBIDFallback, const std::function<void(const ReleaseArtistLink::pointer&)>& func)
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
        utils::forEachQueryResult(query, [&](const ReleaseArtistLink::pointer& link) {
            func(link);
        });
    }

    void ReleaseArtistLink::setArtist(const ObjectPtr<Artist>& artist)
    {
        getDbo(*this)._artist = getDboPtr(artist);
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

    ObjectPtr<Release> ReleaseArtistLink::getRelease() const
    {
        return getDbo(*this)._release;
    }

    ObjectPtr<Artist> ReleaseArtistLink::getArtist() const
    {
        return getDbo(*this)._artist;
    }

    ArtistId ReleaseArtistLink::getArtistId() const
    {
        return getDbo(*this)._artist.id();
    }
} // namespace lms::db
