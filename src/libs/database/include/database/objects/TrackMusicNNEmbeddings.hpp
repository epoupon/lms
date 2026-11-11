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

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include "database/IdType.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/detail/Object.hpp"

LMS_DECLARE_IDTYPE(TrackMusicNNEmbeddingsId)

namespace lms::db
{
    class ISession;

    class TrackMusicNNEmbeddings : public detail::Object<TrackMusicNNEmbeddings, TrackMusicNNEmbeddingsId>
    {
    public:
        static TrackMusicNNEmbeddings create(ISession& session, TrackId track);
        static void destroy(ISession& session, std::span<const TrackMusicNNEmbeddingsId> ids);

        // Find utilities
        static std::size_t getCount(ISession& session);
        static std::optional<TrackMusicNNEmbeddings> find(ISession& session, TrackMusicNNEmbeddingsId id);
        static std::optional<TrackMusicNNEmbeddings> find(ISession& session, TrackId trackId);
        static void find(ISession& session, const std::function<void(const TrackMusicNNEmbeddings&)>& func);
        static void removeAll(ISession& session);

        static void update(ISession& session, const TrackMusicNNEmbeddings& trackMusicNNEmbeddings);

        // Accessors
        std::span<const std::byte> getData() const;
        TrackId getTrackId() const { return _trackId; }

        void setData(std::span<const std::byte> data);

    protected:
        TrackMusicNNEmbeddings() = default;

        std::vector<unsigned char> _data;
        TrackId _trackId;
    };
} // namespace lms::db
