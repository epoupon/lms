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

#include <Wt/WContainerWidget>

#include "database/Types.hpp"

namespace UserInterface {

class Filters;

class Explore : public Wt::WContainerWidget
{
	public:
		Explore(Wt::WContainerWidget *parent = 0);

		Wt::Signal<std::vector<Database::Track::pointer>> tracksAdd;
		Wt::Signal<std::vector<Database::Track::pointer>> tracksPlay;

	private:

		void handleArtistAdd(Database::id_type id);
		void handleArtistPlay(Database::id_type id);
		void handleReleaseAdd(Database::id_type id);
		void handleReleasePlay(Database::id_type id);
		void handleTrackAdd(Database::id_type id);
		void handleTrackPlay(Database::id_type id);

		Filters* _filters;
};

} // namespace UserInterface

