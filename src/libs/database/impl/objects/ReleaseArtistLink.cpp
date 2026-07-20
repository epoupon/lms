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

#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Genre.hpp"
#include "database/objects/Grouping.hpp"
#include "database/objects/Language.hpp"
#include "database/objects/Mood.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"

#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::ReleaseArtistLink)

namespace lms::db
{
    namespace
    {
        Wt::Dbo::Query<Wt::Dbo::ptr<ReleaseArtistLink>> createQuery(Session& session, const ReleaseArtistLink::FindParameters& params)
        {
            session.checkReadTransaction();

            auto query{ session.getDboSession()->query<Wt::Dbo::ptr<ReleaseArtistLink>>("SELECT r_a_l FROM release_artist_link r_a_l") };

            if (params.release.isValid())
                query.where("r_a_l.release_id = ?").bind(params.release);

            if (params.artist.isValid())
                query.where("r_a_l.artist_id = ?").bind(params.artist);

            if (params.sortMethod == ReleaseArtistLinkSortMethod::OriginalDateDesc)
                query.join("track t ON t.release_id = r_a_l.release_id");

            if (params.mbidMatched)
                query.where("r_a_l.artist_mbid_matched = ?").bind(*params.mbidMatched);

            switch (params.sortMethod)
            {
            case ReleaseArtistLinkSortMethod::None:
                break;
            case ReleaseArtistLinkSortMethod::OriginalDateDesc:
                query.orderBy("COALESCE(t.original_date, t.date) DESC");
                break;
            }

            return query;
        }
    } // namespace

    ReleaseArtistLink::ReleaseArtistLink(const ObjectPtr<Release>& release, const ObjectPtr<Artist>& artist, bool artistMBIDMatched)
        : _artistMBIDMatched{ artistMBIDMatched }
        , _release{ getDboPtr(release) }
        , _artist{ getDboPtr(artist) }
    {
    }

    ReleaseArtistLink::pointer ReleaseArtistLink::create(Session& session, const ObjectPtr<Release>& release, const ObjectPtr<Artist>& artist, bool artistMBIDMatched)
    {
        session.checkWriteTransaction();
        return session.getDboSession()->add(std::make_unique<ReleaseArtistLink>(release, artist, artistMBIDMatched));
    }

    std::size_t ReleaseArtistLink::getCount(Session& session)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM track_artist_link"));
    }

    ReleaseArtistLink::pointer ReleaseArtistLink::find(Session& session, ReleaseArtistLinkId id)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<ReleaseArtistLink>>("SELECT r_a_l from release_artist_link r_a_l").where("r_a_l.id = ?").bind(id));
    }

    void ReleaseArtistLink::find(Session& session, const FindParameters& params, std::function<void(const pointer&)> func)
    {
        auto query{ createQuery(session, params) };
        utils::forEachQueryRangeResult(query, params.range, func);
    }

    void ReleaseArtistLink::findArtistNameNoLongerMatch(Session& session, std::optional<Range> range, const std::function<void(const ReleaseArtistLink::pointer&)>& func)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<ReleaseArtistLink>>("SELECT r_a_l from release_artist_link r_a_l") };
        query.join("artist a ON r_a_l.artist_id = a.id");
        query.where("r_a_l.artist_mbid_matched = FALSE");
        query.where("r_a_l.artist_name <> a.name");

        utils::applyRange(query, range);
        utils::forEachQueryResult(query, [&](const ReleaseArtistLink::pointer& link) {
            func(link);
        });
    }

    void ReleaseArtistLink::findWithArtistNameAmbiguity(Session& session, std::optional<Range> range, bool allowArtistMBIDFallback, const std::function<void(const ReleaseArtistLink::pointer&)>& func)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<ReleaseArtistLink>>("SELECT r_a_l from release_artist_link r_a_l") };
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

    void ReleaseArtistLink::setArtist(ObjectPtr<Artist> artist)
    {
        _artist = getDboPtr(artist);
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
} // namespace lms::db
