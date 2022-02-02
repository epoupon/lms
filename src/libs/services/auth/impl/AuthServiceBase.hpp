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

#pragma once

#include <string_view>
#include "services/database/UserId.hpp"

namespace Database
{
	class Db;
	class Session;
}

namespace Auth
{
	class AuthServiceBase
	{
		protected:
			AuthServiceBase(Database::Db& db);

			Database::UserId	getOrCreateUser(std::string_view loginName);
			void				onUserAuthenticated(Database::UserId userId);

			Database::Session&		getDbSession();

		private:
			Database::Db&		_db;
	};
}
