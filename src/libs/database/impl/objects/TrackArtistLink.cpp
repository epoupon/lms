/*
 * Copyright (C) 2013-2016 Emeric Poupon
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

#include "database/objects/TrackArtistLink.hpp"

#include <Wt/Dbo/Impl.h>

#include "core/ILogger.hpp"
#include "core/String.hpp"

#include "ArtistDbo.hpp"
#include "ObjectPtrImpl.hpp"
#include "Session.hpp"
#include "TrackArtistLinkDbo.hpp"
#include "TrackDbo.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::TrackArtistLink)

namespace lms::db
{
    TrackArtistLink::TrackArtistLink(TrackArtistLinkType type, std::string_view subType, bool artistMBIDMatched)
        : _type{ type }
        , _subType{ subType }
        , _artistMBIDMatched{ artistMBIDMatched }
    {
    }

    dbo::TrackArtistLink::TrackArtistLink(const ObjectPtr<db::Track>& track, const ObjectPtr<db::Artist>& artist, TrackArtistLinkType type, std::string_view subType, bool artistMBIDMatched)
        : db::TrackArtistLink{ type, subType, artistMBIDMatched }
        , _track{ getDboPtr(track) }
        , _artist{ getDboPtr(artist) }
    {
    }

    TrackArtistLink::pointer TrackArtistLink::create(ISession& session, const ObjectPtr<Track>& track, const ObjectPtr<Artist>& artist, TrackArtistLinkType type, std::string_view subType, bool artistMBIDMatched)
    {
        return utils::addObject(session, std::make_unique<dbo::TrackArtistLink>(track, artist, type, subType, artistMBIDMatched));
    }

    TrackArtistLink::pointer TrackArtistLink::create(ISession& session, const ObjectPtr<Track>& track, const ObjectPtr<Artist>& artist, TrackArtistLinkType type, bool artistMBIDMatched)
    {
        return create(session, track, artist, type, std::string_view{}, artistMBIDMatched);
    }

    std::size_t TrackArtistLink::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM track_artist_link"));
    }

    TrackArtistLink::pointer TrackArtistLink::find(ISession& session, TrackArtistLinkId id)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<Wt::Dbo::ptr<dbo::TrackArtistLink>>("SELECT t_a_l from track_artist_link t_a_l").where("t_a_l.id = ?").bind(id));
    }

    void TrackArtistLink::find(ISession& session, TrackId trackId, const std::function<void(const TrackArtistLink::pointer& link, const ObjectPtr<Artist>& artist)>& func)
    {
        checkReadTransaction(session);

        using ResultType = std::tuple<Wt::Dbo::ptr<dbo::TrackArtistLink>, Wt::Dbo::ptr<dbo::Artist>>;

        const auto query{ getDboSession(session)->query<ResultType>("SELECT t_a_l, a FROM track_artist_link t_a_l").join("artist a ON t_a_l.artist_id = a.id").where("t_a_l.track_id = ?").bind(trackId) };

        utils::forEachQueryResult(query, [&](const ResultType& result) {
            func(std::get<Wt::Dbo::ptr<dbo::TrackArtistLink>>(result), std::get<Wt::Dbo::ptr<dbo::Artist>>(result));
        });
    }

    core::EnumSet<TrackArtistLinkType> TrackArtistLink::findUsedTypes(ISession& session, ArtistId artistId)
    {
        checkReadTransaction(session);

        const auto query{ getDboSession(session)->query<TrackArtistLinkType>("SELECT DISTINCT type from track_artist_link").where("artist_id = ?").bind(artistId) };

        core::EnumSet<TrackArtistLinkType> res;
        utils::forEachQueryResult(query, [&](TrackArtistLinkType linkType) {
            res.insert(linkType);
        });
        return res;
    }

    void TrackArtistLink::findArtistNameNoLongerMatch(ISession& session, std::optional<Range> range, const std::function<void(const TrackArtistLink::pointer&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::TrackArtistLink>>("SELECT t_a_l FROM track_artist_link t_a_l") };
        query.join("artist a ON t_a_l.artist_id = a.id");
        query.where("t_a_l.artist_mbid_matched = FALSE");
        query.where("t_a_l.artist_name <> a.name");

        utils::applyRange(query, range);
        utils::forEachQueryResult(query, [&](const TrackArtistLink::pointer& link) {
            func(link);
        });
    }

    void TrackArtistLink::findWithArtistNameAmbiguity(ISession& session, std::optional<Range> range, bool allowArtistMBIDFallback, const std::function<void(const TrackArtistLink::pointer&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::TrackArtistLink>>("SELECT t_a_l FROM track_artist_link t_a_l") };
        query.join("artist a ON t_a_l.artist_id = a.id");
        query.where("t_a_l.artist_mbid_matched = FALSE");
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
        utils::forEachQueryResult(query, [&](const TrackArtistLink::pointer& link) {
            func(link);
        });
    }

    void TrackArtistLink::setArtist(const ObjectPtr<Artist>& artist)
    {
        getDbo(*this)._artist = getDboPtr(artist);
    }

    void TrackArtistLink::setArtistName(std::string_view artistName)
    {
        _artistName = core::stringUtils::utf8Truncate(artistName, Artist::maxNameLength);
        LMS_LOG_IF(DB, WARNING, artistName.size() > Artist::maxNameLength, "Artist link name too long, truncated to '" << _artistName << "'");
    }

    void TrackArtistLink::setArtistSortName(std::string_view artistSortName)
    {
        _artistSortName = core::stringUtils::utf8Truncate(artistSortName, Artist::maxNameLength);
        LMS_LOG_IF(DB, WARNING, artistSortName.size() > Artist::maxNameLength, "Artist link sort name too long, truncated to '" << _artistSortName << "'");
    }

    ObjectPtr<Track> TrackArtistLink::getTrack() const
    {
        return getDbo(*this)._track;
    }

    ObjectPtr<Artist> TrackArtistLink::getArtist() const
    {
        return getDbo(*this)._artist;
    }

    ArtistId TrackArtistLink::getArtistId() const
    {
        return getDbo(*this)._artist.id();
    }
} // namespace lms::db
