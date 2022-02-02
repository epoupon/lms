/*
 * Copyright (C) 2019 Emeric Poupon
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

#include "services/auth/IPasswordService.hpp"
#include "AuthServiceBase.hpp"
#include "LoginThrottler.hpp"

namespace Database
{
	class Db;
	class Session;
}

namespace Auth
{

	class PasswordServiceBase : public IPasswordService, public AuthServiceBase
	{
		public:
			PasswordServiceBase(Database::Db& db, std::size_t maxThrottlerEntries, IAuthTokenService& authTokenService);

			PasswordServiceBase(const PasswordServiceBase&) = delete;
			PasswordServiceBase& operator=(const PasswordServiceBase&) = delete;
			PasswordServiceBase(PasswordServiceBase&&) = delete;
			PasswordServiceBase& operator=(PasswordServiceBase&&) = delete;

		protected:
			IAuthTokenService&	getAuthTokenService() { return _authTokenService; }

		private:
			virtual bool	checkUserPassword(std::string_view loginName, std::string_view password) = 0;

			CheckResult		checkUserPassword(const boost::asio::ip::address& clientAddress,
												std::string_view loginName,
												std::string_view password) override;

			std::shared_mutex			_mutex;
			LoginThrottler				_loginThrottler;
			IAuthTokenService&			_authTokenService;
	};

} // namespace Auth
