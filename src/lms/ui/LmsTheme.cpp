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

namespace UserInterface {

LmsTheme::LmsTheme(Database::UITheme theme)
: _theme {theme}
{
}

void
LmsTheme::setTheme(Database::UITheme )
{
/*	if (theme == _theme)
		return;

	// Hack, use the application interface directly since changing theme is not allowed
	const auto currentStyleSheets {getStyleSheets(_theme)};
	for (auto it {std::crbegin(currentStyleSheets)}; it != std::crend(currentStyleSheets); ++it)
		LmsApp->removeStyleSheet(*it);

	_theme = theme;
	for (const auto& styleSheet : getStyleSheets(theme))
		LmsApp->useStyleSheet(styleSheet);
		*/
}
/*
std::vector<Wt::WLinkedCssStyleSheet>
LmsTheme::styleSheets() const
{
/	const std::vector<Wt::WLink> styleSheets {getStyleSheets(_theme)};
	std::vector<Wt::WLinkedCssStyleSheet> res;
	res.reserve(styleSheets.size());

	std::transform(std::cbegin(styleSheets), std::cend(styleSheets), std::back_inserter(res), [](const Wt::WLink& styleSheet) { return Wt::WLinkedCssStyleSheet {styleSheet}; });
	return res;
}

std::vector<Wt::WLink>
LmsTheme::getStyleSheets(Database::UITheme )
{
	switch (theme)
	{
		case Database::UITheme::Dark:
			return
			{
				{"css/fonts.css"},
				{"css/bootstrap-darkly.min.css"},
				{"resources/themes/bootstrap/3/wt.css"},
				{"css/lms.css"},
				{"css/lms-darkly.css"},
			};

		case Database::UITheme::Light:
			return
			{
				{"css/fonts.css"},
				{"css/bootstrap-flatly.min.css"},
				{"resources/themes/bootstrap/3/wt.css"},
				{"css/lms.css"},
				{"css/lms-flatly.css"},
			};
	}
	return {};
}
*/
}
