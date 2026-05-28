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

#include <optional>
#include <span>

#include <Wt/Dbo/Field.h>

#include "database/IdType.hpp"
#include "database/Object.hpp"
#include "database/Types.hpp"
#include "database/objects/TrackId.hpp"

LMS_DECLARE_IDTYPE(TrackMusicNNEmbeddingsId)

namespace lms::db
{
    class Session;
    class Track;

    class TrackMusicNNEmbeddings final : public Object<TrackMusicNNEmbeddings, TrackMusicNNEmbeddingsId>
    {
    public:
        TrackMusicNNEmbeddings() = default;

        // Find utilities
        static std::size_t getCount(Session& session);
        static pointer find(Session& session, TrackMusicNNEmbeddingsId id);
        static pointer find(Session& session, TrackId trackId);
        static RangeResults<TrackMusicNNEmbeddingsId> find(Session& session, std::optional<Range> range = std::nullopt);
        static void find(Session& session, std::function<void(const pointer&)> func);
        static void removeAll(Session& session);

        // Accessors
        std::span<const std::byte> getData() const;
        TrackId getTrackId() const { return _track.id(); }
        Wt::Dbo::ptr<Track> getTrack() const { return _track; }

        void setData(std::span<const std::byte> data);

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _data, "data");
            Wt::Dbo::belongsTo(a, _track, "track", Wt::Dbo::OnDeleteCascade);
        }

    private:
        friend class Session;
        TrackMusicNNEmbeddings(ObjectPtr<Track> track);
        static pointer create(Session& session, ObjectPtr<Track> track);

        std::vector<unsigned char> _data;
        Wt::Dbo::ptr<Track> _track;
    };
} // namespace lms::db
