/*
 * Copyright (C) 2018 Emeric Poupon
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

#include <optional>

#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WTemplate.h>

#include "database/Types.hpp"

namespace UserInterface {

class Filters;
class Tracks : public Wt::WTemplate
{
	public:
		Tracks(Filters* filters);

		Wt::Signal<const std::vector<Database::IdType>&> tracksAdd;
		Wt::Signal<const std::vector<Database::IdType>&> tracksPlay;

	private:
		void refresh();
		void addSome();

		std::vector<Database::IdType> getTracks(std::optional<std::size_t> offset, std::optional<std::size_t> size, bool& moreResults);
		std::vector<Database::IdType> getTracks();

		Wt::WContainerWidget* _tracksContainer;
		Wt::WPushButton* _showMore;
		Wt::WLineEdit* _search;
		Filters* _filters;
};

} // namespace UserInterface

