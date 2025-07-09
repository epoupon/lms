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

#include <chrono>
#include <memory>
#include <optional>
#include <string_view>

#include <Wt/WDateTime.h>
#include <boost/asio/ip/address.hpp>

#include "core/LiteralString.hpp"
#include "database/objects/UserId.hpp"

namespace lms::db
{
    class IDb;
} // namespace lms::db

namespace lms::auth
{
    class IAuthTokenService
    {
    public:
        virtual ~IAuthTokenService() = default;

        struct AuthTokenInfo
        {
            db::UserId userId;
            Wt::WDateTime expiry;
            Wt::WDateTime lastUsed; // if called by processAuthToken, value is before processing
            std::size_t useCount;   // if called by processAuthToken, value is before processing
            std::optional<std::size_t> maxUseCount;
        };

        struct AuthTokenProcessResult
        {
            enum class State
            {
                Granted,
                Throttled,
                Denied,
            };

            State state{ State::Denied };
            std::optional<AuthTokenInfo> authTokenInfo;
        };

        struct DomainParameters
        {
            std::optional<std::size_t> tokenMaxUseCount;
            std::optional<std::chrono::seconds> tokenDuration;
        };

        virtual void registerDomain(core::LiteralString domain, const DomainParameters& params) = 0;

        // Processing an auth token will make its useCount increase by 1. Token is then automatically deleted if its maxUsecount is reached
        virtual AuthTokenProcessResult processAuthToken(core::LiteralString domain, const boost::asio::ip::address& clientAddress, std::string_view tokenValue) = 0;

        virtual void visitAuthTokens(core::LiteralString domain, db::UserId userid, std::function<void(const AuthTokenInfo& info, std::string_view token)> visitor) = 0;

        virtual void createAuthToken(core::LiteralString domain, db::UserId userid, std::string_view token) = 0;
        virtual void clearAuthTokens(core::LiteralString domain, db::UserId userid) = 0;
    };

    std::unique_ptr<IAuthTokenService> createAuthTokenService(db::IDb& db, std::size_t maxThrottlerEntryCount);
} // namespace lms::auth
