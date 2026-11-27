/*
 * Copyright (C) 2026 Emeric Poupon
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

#pragma once

#include <Wt/Dbo/Dbo.h>
#include <Wt/Dbo/Field.h>

#include "database/objects/TrackArtistLink.hpp"

#include "DboAccess.hpp"

namespace lms::db::dbo
{
    class Artist;
    class Track;

    class TrackArtistLink final : public db::TrackArtistLink, public Wt::Dbo::Dbo<TrackArtistLink>
    {
    public:
        TrackArtistLink() = default;
        using db::TrackArtistLink::TrackArtistLink;

        TrackArtistLink(Wt::Dbo::ptr<Track> track, Wt::Dbo::ptr<Artist> artist, TrackArtistLinkType type, std::string_view subType, bool artistMBIDMatched);

        void syncIds()
        {
            _id = id();
            _trackId = _track.id();
            _artistId = _artist.id();
        }
        void applyIds();

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _type, "type");
            Wt::Dbo::field(a, _subType, "subtype");
            Wt::Dbo::field(a, _artistName, "artist_name");
            Wt::Dbo::field(a, _artistSortName, "artist_sort_name");
            Wt::Dbo::field(a, _artistMBIDMatched, "artist_mbid_matched");

            Wt::Dbo::belongsTo(a, _track, "track", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::belongsTo(a, _artist, "artist", Wt::Dbo::OnDeleteCascade);
        }

        Wt::Dbo::ptr<Track> _track;
        Wt::Dbo::ptr<Artist> _artist;
    };
} // namespace lms::db::dbo

namespace lms::db
{
    template<>
    struct DboOf<TrackArtistLink>
    {
        using type = dbo::TrackArtistLink;
    };
} // namespace lms::db
