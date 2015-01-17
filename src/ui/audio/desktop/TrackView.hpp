/*
 * Copyright (C) 2013 Emeric Poupon
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

#ifndef TRACK_HPP
#define TRACK_HPP

#include <Wt/WTableView>
#include <Wt/Dbo/QueryModel>

#include "database/DatabaseHandler.hpp"

#include "Filter.hpp"

namespace UserInterface {
namespace Desktop {

class TrackView : public Wt::WTableView, public Filter
{
	public:

		TrackView( Database::Handler& db, Wt::WContainerWidget* parent = 0);

		// Filter interface
		// Set constraints created by parent filters
		void refresh(Database::SearchFilter& filter);

		// Create constraints for child filters (N/A)
		void getConstraint(Database::SearchFilter& filter) {}

		// Get all the tracks that are currently selected
		void getSelectedTracks(std::vector<Database::Track::id_type>& trackIds);

		std::size_t getNbSelectedTracks(void);

		// Get the first position of the selected tracks (0 if nothing selected)
		int getFirstSelectedTrackPosition(void);

		// Get all the tracks
		void getTracks(std::vector<Database::Track::id_type>& track_ids);

		typedef Wt::Signal<void> SigTrackDoubleClicked;

		SigTrackDoubleClicked&	trackDoubleClicked() { return _sigTrackDoubleClicked; }

	private:

		SigTrackDoubleClicked	_sigTrackDoubleClicked;

		Database::Handler&			_db;

		typedef Database::Track::pointer  ResultType;
		Wt::Dbo::QueryModel< ResultType >	_queryModel;
		Wt::WTableView*				_tableView;

};

} // namespace Desktop
} // namespace UserInterface

#endif

