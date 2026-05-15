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

#include <functional>

#include "database/objects/TrackId.hpp"

#include "math/Vector.hpp"

#include "audio-similarity/Types.hpp"

namespace lms::db
{
    class Session;
}

namespace lms::recommendation
{
    class MusicNNEmbeddingProvider
    {
    public:
        static constexpr std::size_t DimCount{ 200 };
        using Vector = math::Vector<DimCount, FloatType>;

        static std::size_t getCount(db::Session& session);
        static bool getVector(db::Session& session, db::TrackId trackId, Vector& vec);
        static void visitVectors(db::Session& session, const std::function<void(db::TrackId, Vector&)>& visitor);
    };
} // namespace lms::recommendation
