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

#ifndef UI_AUTH_HPP
#define UI_AUTH_HPP

#include <Wt/Auth/AuthService>
#include <Wt/Auth/AbstractUserDatabase>
#include <Wt/Auth/Login>
#include <Wt/Auth/AuthWidget>
#include <Wt/WContainerWidget>

#include "database/DatabaseHandler.hpp"

namespace UserInterface {

class LmsAuth : public Wt::Auth::AuthWidget
{
	public:

		LmsAuth(Database::Handler& db);

		// LoggedInView is delegated to LmsHome
		void createLoggedInView () ;
		void createLoginView ();

	private:


};

} // namespace UserInterface

#endif

