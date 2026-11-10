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
#include <Wt/Dbo/collection.h>

#include "database/objects/Medium.hpp"

#include "DboAccess.hpp"

namespace lms::db::dbo
{
    class Artwork;
    class Release;
    class Track;

    class Medium final : public db::Medium, public Wt::Dbo::Dbo<Medium>
    {
    public:
        Medium() = default;
        using db::Medium::Medium;

        explicit Medium(Wt::Dbo::ptr<Release> release);

        void syncIds()
        {
            _id = id();
            _releaseId = _release.id();
            _preferredArtworkId = _preferredArtwork.id();
        }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _name, "name");
            Wt::Dbo::field(a, _position, "position");
            Wt::Dbo::field(a, _trackCount, "track_count");
            Wt::Dbo::field(a, _media, "media");
            Wt::Dbo::field(a, _replayGain, "replay_gain");

            Wt::Dbo::belongsTo(a, _release, "release", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::belongsTo(a, _preferredArtwork, "preferred_artwork", Wt::Dbo::OnDeleteSetNull);
            Wt::Dbo::hasMany(a, _tracks, Wt::Dbo::ManyToOne, "medium");
        }

        Wt::Dbo::ptr<Release> _release;
        Wt::Dbo::ptr<Artwork> _preferredArtwork;
        Wt::Dbo::collection<Wt::Dbo::ptr<Track>> _tracks; // tracks that match this medium
    };
} // namespace lms::db::dbo

namespace lms::db
{
    template<>
    struct DboOf<Medium>
    {
        using type = dbo::Medium;
    };
} // namespace lms::db
