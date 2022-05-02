/*
 * Copyright (C) 2020 Emeric Poupon
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

#include "LmsTheme.hpp"

#include "utils/Logger.hpp"
#include "LmsApplication.hpp"

namespace UserInterface
{
	std::vector<Wt::WLinkedCssStyleSheet>
	LmsTheme::styleSheets() const
	{
		static const std::vector<Wt::WLinkedCssStyleSheet> files
		{
			{"css/bootstrap.solar.min.css"},
			{"resources/themes/bootstrap/5/wt.min.css"},
			{"css/lms.css"},
		};

		return files;
	}
}
