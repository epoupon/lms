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

/* This file contains some classes in order to get info from file using the libavconv */

#pragma once

#include <string>

#include <boost/asio/ip/address.hpp>

#include "LoginThrottler.hpp"
#include "database/User.hpp"

namespace Database
{
	class Session;
}


namespace Auth {

	class AuthService
	{
		public:

			AuthService(std::size_t maxThrottlerEntries);

			AuthService() = default;
			~AuthService() = default;

			AuthService(const AuthService&) = delete;
			AuthService& operator=(const AuthService&) = delete;
			AuthService(AuthService&&) = delete;
			AuthService& operator=(AuthService&&) = delete;


			// Password services
			enum class PasswordCheckResult
			{
				Match,
				Mismatch,
				Throttled,
			};
			PasswordCheckResult		checkUserPassword(Database::Session& session, const boost::asio::ip::address& clientAddress, const std::string& loginName, const std::string& password);
			Database::User::PasswordHash	hashPassword(const std::string& password) const;
			bool				evaluatePasswordStrength(const std::string& loginName, const std::string& password) const;

		private:

			LoginThrottler _loginThrottler;
	};

}
