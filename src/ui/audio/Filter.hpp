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

#ifndef FILTER_HPP
#define FILTER_HPP

#include <Wt/WSignal>

#include "database/Types.hpp"

namespace UserInterface {

class Filter
{
	public:

		struct Constraint {
			std::vector<std::string>	search;

			typedef std::map<std::string,    std::vector<std::string> > ColumnValues;
			ColumnValues	columnValues;
		};

		Filter() {}
		virtual ~Filter() {}

		// Refresh filter using constraints created by parent filters
		virtual void refresh(Database::SearchFilter& filter) = 0;

		// Update constraints for next filters
		virtual void getConstraint(Database::SearchFilter& filter) = 0;

		// Emitted when a constraint has changed
		Wt::Signal<void>& update() { return _update; };

	protected:

		void emitUpdate()	{ _update.emit(); }

	private:

		Wt::Signal<void> _update;
};

} // namespace UserInterface

#endif
