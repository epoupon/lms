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

#pragma once

#include <vector>

#include <Wt/WBootstrapTheme.h>
#include <Wt/WLinkedCssStyleSheet.h>

#include "lmscore/database/User.hpp"

namespace UserInterface {

class LmsTheme : public Wt::WBootstrapTheme
{
	public:
		LmsTheme(Database::User::UITheme theme);

		void setTheme(Database::User::UITheme theme);

	private:

		std::vector<Wt::WLinkedCssStyleSheet> styleSheets() const override;
		static std::vector<Wt::WLink> getStyleSheets(Database::User::UITheme theme);

		Database::User::UITheme _theme;
};

}
