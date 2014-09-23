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

#include <Wt/WLabel>

#include "SearchFilterWidget.hpp"

namespace UserInterface {

SearchFilterWidget::SearchFilterWidget(Wt::WContainerWidget* parent)
: FilterWidget( parent )
{
}

void
SearchFilterWidget::setText(const std::string& text)
{
	_lastEmittedText = text;
	emitUpdate();
}

// Get constraints created by this filter
void
SearchFilterWidget::getConstraint(Constraint& constraint)
{
	// No active search means no constaint!
	if (!_lastEmittedText.empty()) {
		const std::string bindText ("%%" + _lastEmittedText + "%%");
		constraint.where.And( WhereClause("(track.name like ? or release.name like ? or artist.name like ?)").bind(bindText).bind(bindText).bind(bindText));
	}

	// else no constraint!
}

} // namespace UserInterface

