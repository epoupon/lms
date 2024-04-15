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

#include "database/TrackArtistLink.hpp"

#include "database/Artist.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"

#include "IdTypeTraits.hpp"
#include "Utils.hpp"

namespace lms::db
{
    namespace
    {
        Wt::Dbo::Query<Wt::Dbo::ptr<TrackArtistLink>> createQuery(Session& session, const TrackArtistLink::FindParameters& params)
        {
            session.checkReadTransaction();

            auto query{ session.getDboSession()->query<Wt::Dbo::ptr<TrackArtistLink>>("SELECT t_a_l FROM track_artist_link t_a_l") };

            if (params.linkType)
                query.where("t_a_l.type = ?").bind(*params.linkType);

            if (params.track.isValid())
                query.where("t_a_l.track_id = ?").bind(params.track);

            if (params.artist.isValid())
                query.where("t_a_l.artist_id = ?").bind(params.artist);

            if (params.release.isValid())
            {
                query.join("track t ON t.id = t_a_l.track_id");
                query.where("t.release_id = ?").bind(params.release);
            }

            return query;
        }
    }

    TrackArtistLink::TrackArtistLink(ObjectPtr<Track> track, ObjectPtr<Artist> artist, TrackArtistLinkType type, std::string_view subType)
        : _type{ type }
        , _subType{ subType }
        , _track{ getDboPtr(track) }
        , _artist{ getDboPtr(artist) }
    {
    }

    TrackArtistLink::pointer TrackArtistLink::create(Session& session, ObjectPtr<Track> track, ObjectPtr<Artist> artist, TrackArtistLinkType type, std::string_view subType)
    {
        session.checkWriteTransaction();

        TrackArtistLink::pointer res{ session.getDboSession()->add(std::make_unique<TrackArtistLink>(track, artist, type, subType)) };
        session.getDboSession()->flush();

        return res;
    }

    TrackArtistLink::pointer TrackArtistLink::find(Session& session, TrackArtistLinkId id)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->find<TrackArtistLink>().where("id = ?").bind(id));
    }

    void TrackArtistLink::find(Session& session, TrackId trackId, const std::function<void(const TrackArtistLink::pointer& link, const ObjectPtr<Artist>& artist)>& func)
    {
        session.checkReadTransaction();

        using ResultType = std::tuple < Wt::Dbo::ptr<TrackArtistLink>, Wt::Dbo::ptr<Artist>>;

        const auto query{ session.getDboSession()->query<ResultType>("SELECT t_a_l, a FROM track_artist_link t_a_l")
        .join("artist a ON t_a_l.artist_id = a.id")
        .where("t_a_l.track_id = ?").bind(trackId) };

        utils::forEachQueryResult(query, [&](const ResultType& result)
            {
                func(std::get<Wt::Dbo::ptr<TrackArtistLink>>(result), std::get<Wt::Dbo::ptr<Artist>>(result));
            });
    }

    void TrackArtistLink::find(Session& session, const FindParameters& parameters, const std::function<void(const TrackArtistLink::pointer&)>& func)
    {
        const auto query{ createQuery(session, parameters) };

        utils::forEachQueryResult(query, [&](const TrackArtistLink::pointer& link)
            {
                func(link);
            });
    }

    core::EnumSet<TrackArtistLinkType> TrackArtistLink::findUsedTypes(Session& session)
    {
        session.checkReadTransaction();

        const auto query{ session.getDboSession()->query<TrackArtistLinkType>("SELECT DISTINCT type from track_artist_link") };

        core::EnumSet<TrackArtistLinkType> res;
        utils::forEachQueryResult(query, [&](TrackArtistLinkType linkType)
            {
                res.insert(linkType);
            });
        return res;
    }

    core::EnumSet<TrackArtistLinkType> TrackArtistLink::findUsedTypes(Session& session, ArtistId artistId)
    {
        session.checkReadTransaction();

        const auto query{ session.getDboSession()->query<TrackArtistLinkType>("SELECT DISTINCT type from track_artist_link")
            .where("artist_id = ?").bind(artistId) };

        core::EnumSet<TrackArtistLinkType> res;
        utils::forEachQueryResult(query, [&](TrackArtistLinkType linkType)
            {
                res.insert(linkType);
            });
        return res;
    }
}
