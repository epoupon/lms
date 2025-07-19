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

#include <map>
#include <shared_mutex>

#include "services/auth/IAuthTokenService.hpp"

#include "AuthServiceBase.hpp"
#include "LoginThrottler.hpp"

namespace lms::db
{
    class Session;
}

namespace lms::auth
{
    class AuthTokenService : public IAuthTokenService, public AuthServiceBase
    {
    public:
        AuthTokenService(db::IDb& db, std::size_t maxThrottlerEntryCount);

        ~AuthTokenService() override = default;
        AuthTokenService(const AuthTokenService&) = delete;
        AuthTokenService& operator=(const AuthTokenService&) = delete;
        AuthTokenService(AuthTokenService&&) = delete;
        AuthTokenService& operator=(AuthTokenService&&) = delete;

    private:
        void registerDomain(core::LiteralString domain, const DomainParameters& params) override;
        AuthTokenProcessResult processAuthToken(core::LiteralString domain, const boost::asio::ip::address& clientAddress, std::string_view tokenValue) override;
        void visitAuthTokens(core::LiteralString domain, db::UserId userId, std::function<void(const AuthTokenInfo& info, std::string_view token)> visitor) override;
        void createAuthToken(core::LiteralString domain, db::UserId userId, std::string_view token) override;
        void clearAuthTokens(core::LiteralString domain, db::UserId userId) override;

        std::optional<AuthTokenInfo> processAuthToken(core::LiteralString domain, std::string_view tokenValue);
        const DomainParameters& getDomainParameters(core::LiteralString domain) const;

        std::shared_mutex _mutex;
        std::map<core::LiteralString, DomainParameters> _domainParameters;
        LoginThrottler _loginThrottler;
    };
} // namespace lms::auth
