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
#include "som/Network.hpp"

namespace Similarity {

class FeaturesSearcher
{
	public:

		FeaturesSearcher(Wt::Dbo::Session& session);

		std::vector<Database::IdType> getSimilarTracks(const std::set<Database::IdType>& tracksId, std::size_t maxCount) const;
		std::vector<Database::IdType> getSimilarReleases(Database::IdType releaseId, std::size_t maxCount) const;
		std::vector<Database::IdType> getSimilarArtists(Database::IdType artistId, std::size_t maxCount) const;

		void dump(Wt::Dbo::Session& session, std::ostream& os) const;

	private:

		std::vector<Database::IdType> getSimilarObjects(const std::set<Database::IdType>& ids,
				const SOM::Matrix<std::set<Database::IdType>>& objectsMap,
				const std::map<Database::IdType, std::set<SOM::Coords>>& objectCoords,
				std::size_t maxCount) const;

		std::unique_ptr<SOM::Network>	_network;
		double				_networkRefVectorsDistanceMedian = 0;

		SOM::Matrix<std::set<Database::IdType>> 		_artistsMap;
		std::map<Database::IdType, std::set<SOM::Coords>>	_artistCoords;

		SOM::Matrix<std::set<Database::IdType>> 		_releasesMap;
		std::map<Database::IdType, std::set<SOM::Coords>>	_releaseCoords;

		SOM::Matrix<std::set<Database::IdType>> 		_tracksMap;
		std::map<Database::IdType, std::set<SOM::Coords>>	_trackCoords;

};

} // ns Similarity
