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
#include <boost/optional.hpp>

#include "database/DatabaseHandler.hpp"
#include "database/Types.hpp"
#include "DataNormalizer.hpp"
#include "Network.hpp"

namespace Similarity {

class SOMSearcher
{
	public:

		struct ConstructionParams
		{
			SOM::Network network;
			SOM::DataNormalizer normalizer;
			SOM::Matrix<std::vector<Database::IdType>> tracksMap;
			std::map<Database::IdType, SOM::Coords> trackIdsCoords;
		};

		SOMSearcher(ConstructionParams params);

		std::vector<Database::IdType> getSimilarTracks(const std::vector<Database::IdType>& tracksId, std::size_t maxCount);
		std::vector<Database::IdType> getSimilarReleases(Wt::Dbo::Session& session, Database::IdType releaseId, std::size_t maxCount);
		std::vector<Database::IdType> getSimilarArtists(Wt::Dbo::Session& session, Database::IdType artistId, std::size_t maxCount);

		void dump(Wt::Dbo::Session& session, std::ostream& os) const;

	private:
		boost::optional<SOM::Coords> getBestMatchingCoords(const std::vector<Database::IdType>& tracksIds) const;
		std::vector<SOM::Coords> getMatchingCoords(const std::vector<Database::IdType>& tracksIds) const;

		std::vector<Database::IdType> getReleases(Wt::Dbo::Session& session, const std::vector<SOM::Coords>& coords) const;
		std::vector<Database::IdType> getArtists(Wt::Dbo::Session& session, const std::vector<SOM::Coords>& coords) const;

		SOM::Network _network;
		SOM::DataNormalizer _normalizer;
		SOM::Matrix<std::vector<Database::IdType>> _tracksMap;
		std::map<Database::IdType, SOM::Coords> _trackIdsCoords;
};

} // ns Similarity
