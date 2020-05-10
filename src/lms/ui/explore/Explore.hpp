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

#include <Wt/WTemplate.h>

#include "database/Types.hpp"

namespace UserInterface {

class Filters;

class Explore : public Wt::WTemplate
{
	public:
		Explore(Filters* filters);

		Wt::Signal<std::vector<Database::IdType>> tracksAdd;
		Wt::Signal<std::vector<Database::IdType>> tracksPlay;

	private:

		void handleArtistsAdd(const std::vector<Database::IdType>& artistsId);
		void handleArtistsPlay(const std::vector<Database::IdType>& artistsId);
		void handleReleasesAdd(const std::vector<Database::IdType>& releasesId);
		void handleReleasesPlay(const std::vector<Database::IdType>& releasesId);
		void handleTracksAdd(const std::vector<Database::IdType>& tracksId);
		void handleTracksPlay(const std::vector<Database::IdType>& tracksId);

		Filters* _filters {};
};

} // namespace UserInterface

