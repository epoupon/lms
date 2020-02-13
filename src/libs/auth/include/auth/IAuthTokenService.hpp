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
#include <boost/asio/ip/address.hpp>
#include <Wt/WDateTime.h>

#include "database/Types.hpp"

namespace Database
{
	class Session;
}


namespace Auth {

	class IAuthTokenService
	{
		public:

			// Auth Token services
			struct AuthTokenProcessResult
			{
				enum class State
				{
					Found,
					Throttled,
					NotFound,
				};

				struct AuthTokenInfo
				{
					Database::IdType userId;
					Wt::WDateTime expiry;
				};

				State state {State::NotFound};
				std::optional<AuthTokenInfo>	authTokenInfo {};
			};

			// Removed if found
			virtual AuthTokenProcessResult	processAuthToken(Database::Session& session, const boost::asio::ip::address& clientAddress, const std::string& tokenValue) = 0;
			virtual std::string		createAuthToken(Database::Session& session, Database::IdType userid, const Wt::WDateTime& expiry) = 0;
	};

	std::unique_ptr<IAuthTokenService> createAuthTokenService(std::size_t maxThrottlerEntryCount);

}

