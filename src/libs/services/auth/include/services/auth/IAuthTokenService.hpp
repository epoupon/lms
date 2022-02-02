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

#include <optional>
#include <string>
#include <string_view>
#include <boost/asio/ip/address.hpp>
#include <Wt/WDateTime.h>

#include "services/database/UserId.hpp"

namespace Database
{
	class Db;
	class User;
}

namespace Auth
{
	class IAuthTokenService
	{
		public:
			virtual ~IAuthTokenService() = default;

			// Auth Token services
			struct AuthTokenProcessResult
			{
				enum class State
				{
					Granted,
					Throttled,
					Denied,
				};

				struct AuthTokenInfo
				{
					Database::UserId userId;
					Wt::WDateTime expiry;
				};

				State state {State::Denied};
				std::optional<AuthTokenInfo>	authTokenInfo {};
			};

			// Provided token is only accepted once
			virtual AuthTokenProcessResult	processAuthToken(const boost::asio::ip::address& clientAddress, std::string_view tokenValue) = 0;

			// Returns a one time token
			virtual std::string				createAuthToken(Database::UserId userid, const Wt::WDateTime& expiry) = 0;
			virtual void					clearAuthTokens(Database::UserId userid) = 0;
	};

	std::unique_ptr<IAuthTokenService> createAuthTokenService(Database::Db& db, std::size_t maxThrottlerEntryCount);
}
