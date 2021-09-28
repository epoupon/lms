/*
 * Copyright (C) 2021 Emeric Poupon
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

#include "AuthServiceBase.hpp"

#include "database/Session.hpp"
#include "database/User.hpp"
#include "utils/Logger.hpp"

namespace Auth
{
	Database::UserId
	AuthServiceBase::getOrCreateUser(Database::Session& session, std::string_view loginName)
	{
		auto transaction {session.createUniqueTransaction()};

		Database::User::pointer user {Database::User::getByLoginName(session, loginName)};
		if (!user)
		{
			const Database::UserType type {Database::User::getCount(session) == 0 ? Database::UserType::ADMIN : Database::UserType::REGULAR};

			LMS_LOG(AUTH, DEBUG) << "Creating user '" << loginName << "', admin = " << (type == Database::UserType::ADMIN);

			user = Database::User::create(session, loginName);
			user.modify()->setType(type);
		}

		return user->getId();
	}

	void
	AuthServiceBase::onUserAuthenticated(Database::Session& session, Database::UserId userId)
	{
		auto transaction {session.createUniqueTransaction()};
		Database::User::pointer user {Database::User::getById(session, userId)};
		if (user)
			user.modify()->setLastLogin(Wt::WDateTime::currentDateTime());
	}
}
