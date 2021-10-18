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

#include "services/auth/IAuthTokenService.hpp"
#include "AuthServiceBase.hpp"
#include "LoginThrottler.hpp"

namespace Database
{
	class Session;
}

namespace Auth
{
	class AuthTokenService : public IAuthTokenService, public AuthServiceBase
	{
		public:
			AuthTokenService(Database::Db& db, std::size_t maxThrottlerEntries);

			AuthTokenService(const AuthTokenService&) = delete;
			AuthTokenService& operator=(const AuthTokenService&) = delete;
			AuthTokenService(AuthTokenService&&) = delete;
			AuthTokenService& operator=(AuthTokenService&&) = delete;

		private:
			AuthTokenProcessResult	processAuthToken(const boost::asio::ip::address& clientAddress, std::string_view tokenValue) override;
			std::string				createAuthToken(Database::UserId userId, const Wt::WDateTime& expiry) override;
			void					clearAuthTokens(Database::UserId userId) override;

			std::optional<AuthTokenService::AuthTokenProcessResult::AuthTokenInfo> processAuthToken(std::string_view secret);

			std::shared_mutex	_mutex;
			LoginThrottler		_loginThrottler;
	};
}
