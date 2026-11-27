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

#include "database/objects/TrackMusicNNEmbeddings.hpp"

#include "DboAccess.hpp"

namespace lms::db::dbo
{
    class Track;

    class TrackMusicNNEmbeddings final : public db::TrackMusicNNEmbeddings, public Wt::Dbo::Dbo<TrackMusicNNEmbeddings>
    {
    public:
        TrackMusicNNEmbeddings() = default;
        using db::TrackMusicNNEmbeddings::TrackMusicNNEmbeddings;

        explicit TrackMusicNNEmbeddings(Wt::Dbo::ptr<Track> track);

        void syncIds()
        {
            _id = id();
            _trackId = _track.id();
        }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _data, "data");
            Wt::Dbo::belongsTo(a, _track, "track", Wt::Dbo::OnDeleteCascade);
        }

        Wt::Dbo::ptr<Track> _track;
    };
} // namespace lms::db::dbo

namespace lms::db
{
    template<>
    struct DboOf<TrackMusicNNEmbeddings>
    {
        using type = dbo::TrackMusicNNEmbeddings;
    };
} // namespace lms::db
