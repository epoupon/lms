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

#include <map>
#include <set>

#include "database/DatabaseHandler.hpp"
#include "database/Types.hpp"
#include "som/DataNormalizer.hpp"
#include "som/Network.hpp"
#include "SimilarityFeaturesCache.hpp"

namespace Similarity {


class FeaturesSearcher
{
	public:

		// Use cache
		FeaturesSearcher(Wt::Dbo::Session& session, FeaturesCache cache);

		// Use training (may be very slow)
		FeaturesSearcher(Wt::Dbo::Session& session, bool& stopRequested);

		bool isValid() const;

		bool isTrackClassified(Database::IdType trackId) const;
		bool isReleaseClassified(Database::IdType releaseId) const;
		bool isArtistClassified(Database::IdType artistId) const;

		std::vector<Database::IdType> getSimilarTracks(const std::set<Database::IdType>& tracksId, std::size_t maxCount) const;
		std::vector<Database::IdType> getSimilarReleases(Database::IdType releaseId, std::size_t maxCount) const;
		std::vector<Database::IdType> getSimilarArtists(Database::IdType artistId, std::size_t maxCount) const;

		void dump(Wt::Dbo::Session& session, std::ostream& os) const;

		FeaturesCache toCache() const;

	private:

		using ObjectPositions = std::map<Database::IdType, std::set<SOM::Position>>;

		void init(Wt::Dbo::Session& session,
				SOM::Network network,
				ObjectPositions tracksPosition);

		std::vector<Database::IdType> getSimilarObjects(const std::set<Database::IdType>& ids,
				const SOM::Matrix<std::set<Database::IdType>>& objectsMap,
				const ObjectPositions& objectPosition,
				std::size_t maxCount) const;

		std::unique_ptr<SOM::Network>	_network;
		double				_networkRefVectorsDistanceMedian {};

		SOM::Matrix<std::set<Database::IdType>> 	_artistsMap;
		ObjectPositions					_artistPositions;

		SOM::Matrix<std::set<Database::IdType>> 	_releasesMap;
		ObjectPositions					_releasePositions;

		SOM::Matrix<std::set<Database::IdType>> 	_tracksMap;
		ObjectPositions					_trackPositions;

};

} // ns Similarity
