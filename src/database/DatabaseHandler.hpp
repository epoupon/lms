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

#ifndef DATABASE_HANDLER_HPP
#define DATABASE_HANDLER_HPP

#include <boost/filesystem.hpp>

#include <Wt/Dbo/Dbo>
#include <Wt/Dbo/backend/Sqlite3>
#include <Wt/Auth/Dbo/UserDatabase>
#include <Wt/Auth/Login>
#include <Wt/Auth/PasswordService>

#include "User.hpp"

namespace Database {

		typedef Wt::Auth::Dbo::UserDatabase<AuthInfo> UserDatabase;

// Session living class handling the database and the login
class Handler
{
	public:

		Handler(boost::filesystem::path db);
		~Handler();

		Wt::Dbo::Session& getSession() { return _session; }

		User::pointer getCurrentUser();	// get the current user, may return empty
		User::pointer getUser(const Wt::Auth::User& authUser);	// Get or create the given user

		Wt::Auth::AbstractUserDatabase& getUserDatabase();
		Wt::Auth::Login& getLogin() { return _login; }

		// Long living shared associated services
		static void configureAuth();

		static const Wt::Auth::AuthService& getAuthService();
		static const Wt::Auth::PasswordService& getPasswordService();


	private:

		Wt::Dbo::backend::Sqlite3	_dbBackend;
		Wt::Dbo::Session		_session;

		UserDatabase*			_users;
		Wt::Auth::Login 		_login;

};

} // namespace Database

#endif

