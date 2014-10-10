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

#ifndef SEARCH_FILTER_WIDGET_PP
#define SEARCH_FILTER_WIDGET_PP

#include <Wt/WLineEdit>

#include "FilterWidget.hpp"

namespace UserInterface {

class SearchFilterWidget : public FilterWidget {
	public:
		SearchFilterWidget(Wt::WContainerWidget* parent = 0);

		void setText(const std::string& text);

		// Set constraint on this filter
		void refresh(const Constraint& constraint) {}

		// Get constraints created by this filter
		void getConstraint(Constraint& constraint);

	private:

		void handleKeyWentUp(void);

		std::string	 _lastEmittedText;
};

} // namespace UserInterface

#endif

