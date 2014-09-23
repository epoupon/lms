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

#ifndef TABLE_VIEW_FILTER_WIDGET_HPP
#define TABLE_VIEW_FILTER_WIDGET_HPP


#include <Wt/Dbo/QueryModel>
#include <Wt/WTableView>

#include "FilterWidget.hpp"
#include "database/DatabaseHandler.hpp"

namespace UserInterface {

class TableFilterWidget : public FilterWidget
{

	public:
		TableFilterWidget(Database::Handler& db, std::string table, std::string field, Wt::WContainerWidget* parent = 0);

		// Set constraints on this filter
		virtual void refresh(const Constraint& constraint);

		// Get constraints created by this filter
		virtual void getConstraint(Constraint& constraint);

	protected:

		Database::Handler&			_db;
		const std::string			_table;
		const std::string			_field;

		// Name, track count, special value that means 'all' if set to 1
		typedef boost::tuple<std::string, int, int>  ResultType;
		Wt::Dbo::QueryModel< ResultType >	_queryModel;

		Wt::WTableView*				_tableView;
};

} // namespace UserInterface

#endif

