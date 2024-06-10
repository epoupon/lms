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

#include <Wt/WDateTime.h>
#include <boost/asio/ip/address.hpp>
#include <optional>
#include <string>
#include <string_view>

#include "database/UserId.hpp"

namespace lms::db
{
    class Db;
    class User;
} // namespace lms::db

namespace lms::auth
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
                db::UserId userId;
                Wt::WDateTime expiry;
            };

            State state{ State::Denied };
            std::optional<AuthTokenInfo> authTokenInfo{};
        };

        // Provided token is only accepted once
        virtual AuthTokenProcessResult processAuthToken(const boost::asio::ip::address& clientAddress, std::string_view tokenValue) = 0;

        // Returns a one time token
        virtual std::string createAuthToken(db::UserId userid, const Wt::WDateTime& expiry) = 0;
        virtual void clearAuthTokens(db::UserId userid) = 0;
    };

    std::unique_ptr<IAuthTokenService> createAuthTokenService(db::Db& db, std::size_t maxThrottlerEntryCount);
} // namespace lms::auth
