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

#ifndef TRACK_WIDGET_HPP
#define TRACK_WIDGET_HPP

#include <Wt/WTableView>
#include <Wt/Dbo/QueryModel>

#include "database/DatabaseHandler.hpp"
#include "database/AudioTypes.hpp"

#include "FilterWidget.hpp"

namespace UserInterface {

class TrackWidget : public FilterWidget
{
	public:

		TrackWidget( Database::Handler& db, Wt::WContainerWidget* parent = 0);

		// Set constraints created by parent filters
		void refresh(const Constraint& constraint);

		// Create constraints for child filters (N/A)
		void getConstraint(Constraint& constraint) {}

		void selectNextTrack(void);	// Emit a trackSelected() if success

		// Signals
		Wt::Signal< boost::filesystem::path >& trackSelected() { return _trackSelected; }

	private:

		void handleTrackSelected();

		Wt::Signal< boost::filesystem::path > _trackSelected;

		Database::Handler&			_db;

		typedef boost::tuple<Database::Track::pointer, Database::Release::pointer, Database::Artist::pointer>  ResultType;
		Wt::Dbo::QueryModel< ResultType >	_queryModel;
		Wt::WTableView*				_tableView;

		void updateStats(void);

		Wt::WText*	_trackStats;

};

} // namespace UserInterface

#endif

