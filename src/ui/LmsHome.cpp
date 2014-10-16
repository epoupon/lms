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

#include <Wt/WStackedWidget>
#include <Wt/WMenu>
#include <Wt/WNavigationBar>
#include <Wt/WPopupMenu>
#include <Wt/WPopupMenuItem>
#include <Wt/WVBoxLayout>
#include <Wt/Auth/Identity>

#include "settings/Settings.hpp"

#include "LmsHome.hpp"

namespace UserInterface {

LmsHome::LmsHome(SessionData& sessionData, Wt::WContainerWidget* parent)
: Wt::WContainerWidget(parent),
 _sessionData(sessionData)
{
	const Wt::Auth::User& user = sessionData.getDatabaseHandler().getLogin().user();



}

void
LmsHome::handleUserMenuSelected( Wt::WMenuItem* item)
{
	if (item && item->text() == "Logout") {
		_sessionData.getDatabaseHandler().getLogin().logout();
	}
}

void
LmsHome::handleSearch(void)
{
	// TODO Check currently selected menu item and search it
	_audioWidget->search( _searchEdit->text().toUTF8() );
}

} // namespace UserInterface
