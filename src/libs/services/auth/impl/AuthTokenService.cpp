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

#include "AuthTokenService.hpp"

#include <Wt/Auth/HashFunction.h>
#include <Wt/WRandom.h>

#include "core/ILogger.hpp"
#include "database/Session.hpp"
#include "database/objects/AuthToken.hpp"
#include "database/objects/User.hpp"
#include "services/auth/Types.hpp"

namespace lms::auth
{
    namespace
    {
        AuthTokenService::AuthTokenInfo createAuthTokenInfo(const db::AuthToken::pointer& authToken)
        {
            return AuthTokenService::AuthTokenInfo{
                .userId = authToken->getUser()->getId(),
                .expiry = authToken->getExpiry(),
                .lastUsed = authToken->getLastUsed(),
                .useCount = authToken->getUseCount(),
                .maxUseCount = authToken->getMaxUseCount(),
            };
        }
    } // namespace

    std::unique_ptr<IAuthTokenService> createAuthTokenService(db::IDb& db, std::size_t maxThrottlerEntryCount)
    {
        return std::make_unique<AuthTokenService>(db, maxThrottlerEntryCount);
    }

    AuthTokenService::AuthTokenService(db::IDb& db, std::size_t maxThrottlerEntryCount)
        : AuthServiceBase{ db }
        , _loginThrottler{ maxThrottlerEntryCount }
    {
    }

    void AuthTokenService::registerDomain(core::LiteralString domain, const DomainParameters& params)
    {
        [[maybe_unused]] auto [it, inserted]{ _domainParameters.emplace(domain, params) };
        if (!inserted)
            throw Exception{ "Auth token domain already registered!" };
    }

    void AuthTokenService::createAuthToken(core::LiteralString domain, db::UserId userId, std::string_view token)
    {
        const DomainParameters& params{ getDomainParameters(domain) };

        db::Session& session{ getDbSession() };
        const auto now{ Wt::WDateTime::currentDateTime() };
        const auto expiry{ params.tokenDuration ? now.addSecs(std::chrono::duration_cast<std::chrono::seconds>(params.tokenDuration.value()).count()) : Wt::WDateTime{} };

        {
            auto transaction{ session.createWriteTransaction() };

            const db::User::pointer user{ db::User::find(session, userId) };
            if (!user)
                throw Exception{ "User deleted" };

            const db::AuthToken::pointer authToken{ session.create<db::AuthToken>(domain.str(), token, expiry, params.tokenMaxUseCount, user) };

            LMS_LOG(UI, DEBUG, "Created auth token for user '" << user->getLoginName() << "', expiry = " << authToken->getExpiry().toString() << ", maxUseCount = " << (authToken->getMaxUseCount() ? std::to_string(*authToken->getMaxUseCount()) : "<unset>"));

            // TODO per domain
            if (user->getAuthTokensCount() >= 50)
                db::AuthToken::removeExpiredTokens(session, domain.str(), Wt::WDateTime::currentDateTime());
        }
    }

    std::optional<AuthTokenService::AuthTokenInfo> AuthTokenService::processAuthToken(core::LiteralString domain, std::string_view token)
    {
        db::Session& session{ getDbSession() };
        auto transaction{ session.createWriteTransaction() };

        db::AuthToken::pointer authToken{ db::AuthToken::find(session, domain.str(), token) };
        if (!authToken)
            return std::nullopt;

        if (authToken->getExpiry().isValid() && authToken->getExpiry() < Wt::WDateTime::currentDateTime())
        {
            authToken.remove();
            return std::nullopt;
        }

        LMS_LOG(UI, DEBUG, "Found auth token for user '" << authToken->getUser()->getLoginName() << "' on domain '" << domain.str() << "'");

        AuthTokenInfo res{ createAuthTokenInfo(authToken) };

        const std::size_t tokenUseCount{ authToken.modify()->incUseCount() };
        authToken.modify()->setLastUsed(Wt::WDateTime::currentDateTime());

        if (auto maxUseCount{ authToken->getMaxUseCount() })
        {
            if (*maxUseCount >= tokenUseCount)
                authToken.remove();
        }

        return res;
    }

    AuthTokenService::AuthTokenProcessResult AuthTokenService::processAuthToken(core::LiteralString domain, const boost::asio::ip::address& clientAddress, std::string_view tokenValue)
    {
        // Do not waste too much resource on brute force attacks (optim)
        {
            std::shared_lock lock{ _mutex };

            if (_loginThrottler.isClientThrottled(clientAddress))
                return AuthTokenProcessResult{ .state = AuthTokenProcessResult::State::Throttled, .authTokenInfo = std::nullopt };
        }

        auto res{ processAuthToken(domain, tokenValue) };
        {
            std::unique_lock lock{ _mutex };

            if (_loginThrottler.isClientThrottled(clientAddress))
                return AuthTokenProcessResult{ .state = AuthTokenProcessResult::State::Throttled, .authTokenInfo = std::nullopt };

            if (!res)
            {
                _loginThrottler.onBadClientAttempt(clientAddress);
                return AuthTokenProcessResult{ .state = AuthTokenProcessResult::State::Denied, .authTokenInfo = std::nullopt };
            }

            _loginThrottler.onGoodClientAttempt(clientAddress);
            onUserAuthenticated(res->userId);
            return AuthTokenProcessResult{ .state = AuthTokenProcessResult::State::Granted, .authTokenInfo = res };
        }
    }

    void AuthTokenService::visitAuthTokens(core::LiteralString domain, db::UserId userId, std::function<void(const AuthTokenInfo& info, std::string_view token)> visitor)
    {
        db::Session& session{ getDbSession() };

        {
            auto transaction{ session.createReadTransaction() };

            db::AuthToken::find(session, domain.str(), userId, [&](const db::AuthToken::pointer& authToken) {
                const AuthTokenInfo info{ createAuthTokenInfo(authToken) };
                visitor(info, authToken->getValue());
            });
        }
    }

    void AuthTokenService::clearAuthTokens(core::LiteralString domain, db::UserId userId)
    {
        db::Session& session{ getDbSession() };

        {
            auto transaction{ session.createWriteTransaction() };
            db::AuthToken::clearUserTokens(session, domain.str(), userId);
        }
    }

    const AuthTokenService::DomainParameters& AuthTokenService::getDomainParameters(core::LiteralString domain) const
    {
        auto it{ _domainParameters.find(domain) };
        if (it == std::cend(_domainParameters))
            throw Exception{ "Invalid auth token domain" };

        return it->second;
    }
} // namespace lms::auth
