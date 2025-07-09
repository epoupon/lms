/*
 * Copyright (C) 2018 Emeric Poupon
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

#include <filesystem>
#include <unordered_map>

#include "database/objects/TrackId.hpp"
#include "som/Network.hpp"

namespace lms::recommendation
{

    class FeaturesEngineCache
    {
    public:
        static void invalidate();

        static std::optional<FeaturesEngineCache> read();
        void write() const;

    private:
        using TrackPositions = std::unordered_map<db::TrackId, std::vector<som::Position>>;

        FeaturesEngineCache(som::Network network, TrackPositions trackPositions);

        static std::optional<som::Network> createNetworkFromCacheFile(const std::filesystem::path& path);
        static std::optional<TrackPositions> createObjectPositionsFromCacheFile(const std::filesystem::path& path);
        static bool objectPositionToCacheFile(const TrackPositions& trackPositions, const std::filesystem::path& path);

        friend class FeaturesEngine;

        som::Network _network;
        TrackPositions _trackPositions;
    };

} // namespace lms::recommendation
