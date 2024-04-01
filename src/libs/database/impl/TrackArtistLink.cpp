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
        Wt::Dbo::Query<TrackArtistLinkId> createQuery(Session& session, const TrackArtistLink::FindParameters& params)
        {
            session.checkReadTransaction();

            auto query{ session.getDboSession()->query<TrackArtistLinkId>("SELECT DISTINCT t_a_l.id FROM track_artist_link t_a_l") };

            if (params.linkType)
                query.where("t_a_l.type = ?").bind(*params.linkType);

            if (params.track.isValid() || params.release.isValid())
                query.join("track t ON t.id = t_a_l.track_id");

            if (params.artist.isValid())
                query.join("artist a ON a.id = t_a_l.artist_id");

            if (params.release.isValid())
                query.where("t.release_id = ?").bind(params.release);

            if (params.track.isValid())
                query.where("t.id = ?").bind(params.track);

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

    RangeResults<TrackArtistLinkId> TrackArtistLink::find(Session& session, const FindParameters& params)
    {
        session.checkReadTransaction();

        auto query{ createQuery(session, params) };
        return utils::execRangeQuery<TrackArtistLinkId>(query, params.range);
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
