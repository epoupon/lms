/*
 * Copyright (C) 2015 Emeric Poupon
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
#include <Wt/WSignal>

namespace UserInterface {
namespace Mobile {

class TrackSearch : public Wt::WContainerWidget
{
	public:

		TrackSearch(Wt::WContainerWidget *parent = 0);

		void search(Database::SearchFilter filter, size_t nb);

		// Slots
		Wt::Signal<Database::Track::id_type>&	trackPlay() { return _sigTrackPlay;}
		Wt::Signal<void>& moreSelected() { return _sigMoreSelected;}

	private:

		Wt::Signal<Database::Track::id_type> _sigTrackPlay;
		Wt::Signal<void> _sigMoreSelected;

		void clear(void);
		void addResults(size_t nb);

		Wt::WTemplate*	_showMore;

		Database::SearchFilter	_filter;
		std::size_t		_nbTracks;
		Wt::WContainerWidget*	_container;
};

} // namespace Mobile
} // namespace UserInterface

