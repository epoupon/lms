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
#include <unordered_set>

#include "database/Types.hpp"
#include "som/Network.hpp"

namespace Recommendation {

class FeaturesClassifierCache
{
	public:
		static void invalidate();

		static std::optional<FeaturesClassifierCache> read();
		void write() const;

	private:
		using ObjectPositions = std::unordered_map<Database::IdType, std::unordered_set<SOM::Position>>;

		FeaturesClassifierCache(SOM::Network network, ObjectPositions trackPositions);

		static std::optional<SOM::Network> createNetworkFromCacheFile(const std::filesystem::path& path);
		static std::optional<ObjectPositions> createObjectPositionsFromCacheFile(const std::filesystem::path& path);
		static bool objectPositionToCacheFile(const ObjectPositions& objectsPosition, const std::filesystem::path& path);

		friend class FeaturesClassifier;

		SOM::Network		_network;
		ObjectPositions		_trackPositions;
};

} // namespace Recommendation
