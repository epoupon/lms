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

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>

#include "KeywordSearchFilter.hpp"

namespace UserInterface {
namespace Desktop {

KeywordSearchFilter::KeywordSearchFilter()
{
}

void
KeywordSearchFilter::setText(const std::string& text)
{
	_lastEmittedText = text;
	emitUpdate();
}

// Get constraints created by this filter
void
KeywordSearchFilter::getConstraint(Database::SearchFilter& filter)
{
	// No active search means no constaint!
	if (!_lastEmittedText.empty()) {
		std::vector<std::string> values;
		boost::algorithm::split(values, _lastEmittedText, boost::is_any_of(" "), boost::token_compress_on);

		// For each part, do a global search on all searchable fields
		for (const std::string& value : values)
		{
			filter.nameLikeMatch[Database::SearchFilter::Field::Artist].push_back(value);
			filter.nameLikeMatch[Database::SearchFilter::Field::Release].push_back(value);
			filter.nameLikeMatch[Database::SearchFilter::Field::Genre].push_back(value);
			filter.nameLikeMatch[Database::SearchFilter::Field::Track].push_back(value);
		}
	}
}

} // namespace Desktop
} // namespace UserInterface

