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

#include <Wt/WContainerWidget>
#include <Wt/WLineEdit>
#include <Wt/WTemplate>

namespace UserInterface {

class Filters;
class Tracks : public Wt::WContainerWidget
{
	public:
		Tracks(Filters* filters, Wt::WContainerWidget* parent = 0);

		Wt::Signal<Database::id_type> trackAdd;
		Wt::Signal<Database::id_type> trackPlay;

	private:
		void refresh();
		void addSome();

		Wt::WContainerWidget* _tracksContainer;
		Wt::WTemplate* _showMore;
		Wt::WLineEdit* _search;
		Filters* _filters;
};

} // namespace UserInterface

