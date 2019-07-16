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

#pragma once

#include <shared_mutex>
#include <mutex>
#include <boost/filesystem.hpp>
#include <memory>

#include <Wt/Dbo/Dbo.h>
#include <Wt/Dbo/SqlConnectionPool.h>
#include <Wt/Auth/Dbo/AuthInfo.h>

#include <Wt/Auth/Dbo/UserDatabase.h>
#include <Wt/Auth/Login.h>
#include <Wt/Auth/PasswordService.h>

#include "User.hpp"

namespace Database {

using AuthInfo = Wt::Auth::Dbo::AuthInfo<User>;
using UserDatabase = Wt::Auth::Dbo::UserDatabase<AuthInfo>;

class UniqueTransaction
{
	public:
		~UniqueTransaction();

	private:
		friend class Session;
		UniqueTransaction(std::shared_timed_mutex& mutex, Wt::Dbo::Session& session);

		std::unique_lock<std::shared_timed_mutex> _lock;
		Wt::Dbo::Transaction _transaction;
};

class SharedTransaction
{
	public:
		~SharedTransaction();

	private:
		friend class Session;
		SharedTransaction(std::shared_timed_mutex& mutex, Wt::Dbo::Session& session);

		std::shared_lock<std::shared_timed_mutex> _lock;
		Wt::Dbo::Transaction _transaction;
};

class Session
{
	public:
		Session(const Session&) = delete;
		Session(Session&&) = delete;
		Session& operator=(const Session&) = delete;
		Session& operator=(Session&&) = delete;

		std::unique_ptr<UniqueTransaction> createUniqueTransaction();
		std::unique_ptr<SharedTransaction> createSharedTransaction();

		void checkUniqueLocked();
		void checkSharedLocked();

		void optimize();

		// User management
		Wt::Dbo::ptr<User>	getLoggedUser();	// get the current user, may return empty
		Wt::Dbo::ptr<User>	getUser(const std::string& loginName);
		std::string		getUserLoginName(Wt::Dbo::ptr<User> user);
		Wt::Dbo::ptr<User>	createUser(const std::string& loginName, const std::string& password);
		void			removeUser(Wt::Dbo::ptr<User> user);
		bool			checkUserPassword(const std::string& loginName, const std::string& password);
		void			updateUserPassword(Wt::Dbo::ptr<User> user, const std::string& password);
		Wt::WDateTime		getUserLastLoginAttempt(Wt::Dbo::ptr<User> user);

		Wt::Auth::AbstractUserDatabase& getUserDatabase();
		Wt::Auth::Login& getLogin() { return _login; } // TODO move

		// Long living shared associated services
		static void configureAuth();
		static const Wt::Auth::AuthService& getAuthService();
		static const Wt::Auth::PasswordService& getPasswordService();

		Wt::Dbo::Session& getDboSession() { return _session; }

	private:
		friend class Database;

		Session(std::shared_timed_mutex& mutex, Wt::Dbo::SqlConnectionPool& connectionPool);

		void doDatabaseMigrationIfNeeded();
		void prepareTables(); // need to run only once at startup

		 Wt::Dbo::ptr<User> getUser(const Wt::Auth::User& authUser);

		std::shared_timed_mutex&	_mutex;
		Wt::Dbo::Session		_session;
		std::unique_ptr<UserDatabase>	_users;
		Wt::Auth::Login 		_login;

};

} // namespace Database


