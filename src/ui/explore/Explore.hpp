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
		Explore();

		Wt::Signal<std::vector<Database::IdType>> tracksAdd;
		Wt::Signal<std::vector<Database::IdType>> tracksPlay;

	private:

		void handleArtistAdd(Database::IdType artistId);
		void handleArtistPlay(Database::IdType artistId);
		void handleReleaseAdd(Database::IdType releaseId);
		void handleReleasePlay(Database::IdType releaseId);
		void handleTrackAdd(Database::IdType trackId);
		void handleTrackPlay(Database::IdType trackId);
		void handleTracksAdd(const std::vector<Database::IdType>& trackIds);
		void handleTracksPlay(const std::vector<Database::IdType>& trackIds);

		Filters* _filters;
};

} // namespace UserInterface

