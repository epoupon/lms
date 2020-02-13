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

#include <optional>
#include <string>

#include <boost/asio/ip/address.hpp>

#include "database/User.hpp"
#include "database/Types.hpp"

namespace Database
{
	class Session;
}


namespace Auth {

	class IPasswordService
	{
		public:

			virtual ~IPasswordService() = default;

			// Password services
			enum class PasswordCheckResult
			{
				Match,
				Mismatch,
				Throttled,
			};
			virtual PasswordCheckResult	checkUserPassword(Database::Session& session, const boost::asio::ip::address& clientAddress, const std::string& loginName, const std::string& password) = 0;
			virtual Database::User::PasswordHash	hashPassword(const std::string& password) const = 0;
			virtual bool				evaluatePasswordStrength(const std::string& loginName, const std::string& password) const = 0;
	};

	std::unique_ptr<IPasswordService> createPasswordService(std::size_t maxThrottlerEntryCount);

}

