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

#ifndef FILTER_WIDGET_HPP
#define FILTER_WIDGET_HPP

#include <boost/foreach.hpp>
#include <string>
#include <list>

#include <Wt/WSignal>
#include <Wt/WContainerWidget>

#include "database/SqlQuery.hpp"

namespace UserInterface {

class FilterWidget : public Wt::WContainerWidget {

	public:

		struct Constraint {
			WhereClause	where;		// WHERE SQL clause
		};

		FilterWidget(Wt::WContainerWidget* parent = 0) : Wt::WContainerWidget(parent) {}
		virtual ~FilterWidget() {}

		// Refresh filter Widget using constraints created by parent filters
		virtual void refresh(const Constraint& constraint) = 0;

		// Update constraints for child filters
		virtual void getConstraint(Constraint& constraint) = 0;

		// Emitted when a constraint has changed
		Wt::Signal<void>& update() { return _update; };

	protected:

		void emitUpdate()	{ _update.emit(); }

	private:

		Wt::Signal<void> _update;
};

} // namespace UserInterface

#endif
