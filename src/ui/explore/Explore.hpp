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

#include "database/Track.hpp"
#include "database/Types.hpp"

namespace UserInterface {

class Filters;

class Explore : public Wt::WTemplate
{
	public:
		Explore();

		Wt::Signal<std::vector<Database::Track::pointer>> tracksAdd;
		Wt::Signal<std::vector<Database::Track::pointer>> tracksPlay;

	private:

		void handleArtistAdd(Database::IdType id);
		void handleArtistPlay(Database::IdType id);
		void handleReleaseAdd(Database::IdType id);
		void handleReleasePlay(Database::IdType id);
		void handleTrackAdd(Database::IdType id);
		void handleTrackPlay(Database::IdType id);
		void handleTracksAdd(std::vector<Database::Track::pointer> tracks);
		void handleTracksPlay(std::vector<Database::Track::pointer> tracks);

		Filters* _filters;
};

} // namespace UserInterface

