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

#include <Wt/WContainerWidget.h>
#include <Wt/WTemplate.h>
#include <Wt/WLineEdit.h>
#include <Wt/WSignal.h>

#include "database/Types.hpp"

namespace UserInterface {

class Filters;

class Artists : public Wt::WTemplate
{
	public:
		Artists(Filters* filters);

		Wt::Signal<Database::IdType> artistAdd;
		Wt::Signal<Database::IdType> artistPlay;

	private:
		void refreshRecentlyAdded();
		void refreshMostPlayed();
		void refresh();
		void addSome();

		Filters* _filters;
		Wt::WTemplate* _showMore;
		Wt::WLineEdit* _search;
		Wt::WContainerWidget* _container;
		Wt::WContainerWidget* _mostPlayedContainer;
		Wt::WContainerWidget* _recentlyAddedContainer;
};

} // namespace UserInterface

