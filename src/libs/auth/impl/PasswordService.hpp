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
#include "auth/IPasswordService.hpp"

#include "LoginThrottler.hpp"

namespace Database
{
	class Session;
}


namespace Auth {

	class PasswordService : public IPasswordService
	{
		public:

			PasswordService(std::size_t maxThrottlerEntries);

			PasswordService(const PasswordService&) = delete;
			PasswordService& operator=(const PasswordService&) = delete;
			PasswordService(PasswordService&&) = delete;
			PasswordService& operator=(PasswordService&&) = delete;

		private:

			bool isAuthModeSupported(Database::User::AuthMode authMode) const override;
			PasswordCheckResult		checkUserPassword(Database::Session& session, const boost::asio::ip::address& clientAddress, const std::string& loginName, const std::string& password) override;
			Database::User::PasswordHash	hashPassword(const std::string& password) const override;
			bool				evaluatePasswordStrength(const std::string& loginName, const std::string& password) const override;

			std::shared_timed_mutex	_mutex;
			LoginThrottler	_loginThrottler;
	};

}
