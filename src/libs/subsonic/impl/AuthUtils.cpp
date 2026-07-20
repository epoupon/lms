/*
 * Copyright (C) 2026 Emeric Poupon
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

#include "AuthUtils.hpp"

#include <string>

#include <boost/asio/ip/address.hpp>

#include "core/Md5.hpp"
#include "core/Service.hpp"
#include "core/String.hpp"
#include "core/Utils.hpp"

#include "database/Session.hpp"
#include "services/auth/IAuthTokenService.hpp"

#include "ParameterParsing.hpp"
#include "SubsonicResponse.hpp"

namespace lms::api::subsonic::utils
{
    namespace
    {
        // Finds, among the user's stored "subsonic" API keys, the one that satisfies the legacy "t"+"s" scheme, if any (empty string if none)
        std::string findMatchingApiKeyForAuthToken(db::UserId userId, std::string_view salt, std::string_view token)
        {
            std::string matchedApiKey;

            core::Service<auth::IAuthTokenService>::get()->visitAuthTokens("subsonic", userId, [&](const auth::IAuthTokenService::AuthTokenInfo&, std::string_view apiKey) {
                if (matchedApiKey.empty() && checkAuthToken(apiKey, salt, token))
                    matchedApiKey = apiKey;
            });

            return matchedApiKey;
        }

        // Returns null if no such user exists
        db::User::pointer getUserFromLoginName(db::Session& session, std::string_view loginName)
        {
            auto transaction{ session.createReadTransaction() };

            return db::User::find(session, loginName);
        }
    } // namespace

    AuthenticationRequest parseAndValidateAuthenticationRequest(const Wt::Http::ParameterMap& parameters)
    {
        const std::optional<std::string> user{ getParameterAs<std::string>(parameters, "u") };
        const std::optional<std::string> password{ getParameterAs<std::string>(parameters, "p") };
        const std::optional<std::string> token{ getParameterAs<std::string>(parameters, "t") };
        const std::optional<std::string> salt{ getParameterAs<std::string>(parameters, "s") };
        const std::optional<std::string> apiKey{ getParameterAs<std::string>(parameters, "apiKey") };

        const bool passwordAuthRequested{ password.has_value() };
        const bool tokenAuthRequested{ token.has_value() || salt.has_value() };

        if (passwordAuthRequested && tokenAuthRequested)
            throw MultipleConflictingAuthenticationMechanismsProvidedError{};

        if (tokenAuthRequested)
        {
            if (!user)
                throw RequiredParameterMissingError{ "u" };
            if (!token)
                throw RequiredParameterMissingError{ "t" };
            if (!salt)
                throw RequiredParameterMissingError{ "s" };
            if (apiKey)
                throw MultipleConflictingAuthenticationMechanismsProvidedError{};
            return TokenAuthentication{ .user = *user, .token = *token, .salt = *salt };
        }

        if (passwordAuthRequested)
        {
            if (!user)
                throw RequiredParameterMissingError{ "u" };
            if (apiKey)
                throw MultipleConflictingAuthenticationMechanismsProvidedError{};
            return PasswordAuthentication{ .user = *user, .password = *password };
        }

        if (user)
            throw RequiredParameterMissingError{ "p" };
        if (!apiKey)
            throw RequiredParameterMissingError{ "apiKey" };

        return ApiKeyAuthentication{ .apiKey = *apiKey };
    }

    bool checkAuthToken(std::string_view apiKey, std::string_view salt, std::string_view token)
    {
        std::string payload;
        payload.reserve(apiKey.size() + salt.size());
        payload += apiKey;
        payload += salt;

        return core::stringUtils::stringCaseInsensitiveEqual(core::stringUtils::bufferToHexString(core::md5(payload)), token);
    }

    db::User::pointer getUserFromUserId(db::Session& session, db::UserId userId)
    {
        auto transaction{ session.createReadTransaction() };

        if (db::User::pointer user{ db::User::find(session, userId) })
            return user;

        throw UserNotAuthorizedError{};
    }

    db::UserId authenticateUser(const Wt::Http::Request& request, db::Session& session)
    {
        const AuthenticationRequest authRequest{ parseAndValidateAuthenticationRequest(request.getParameterMap()) };

        const auto clientAddress{ boost::asio::ip::make_address(request.clientAddress()) };
        auto& authTokenService{ *core::Service<auth::IAuthTokenService>::get() };

        const std::string authToken{
            std::visit(core::utils::overloads{
                           [&](const PasswordAuthentication& auth) {
                               return decodePasswordIfNeeded(auth.password);
                           },
                           [&](const ApiKeyAuthentication& auth) {
                               return auth.apiKey;
                           },
                           [&](const TokenAuthentication& auth) {
                               if (authTokenService.isClientThrottled(clientAddress))
                                   throw LoginThrottledGenericError{};

                               const db::User::pointer authUser{ getUserFromLoginName(session, auth.user) };
                               return authUser ? findMatchingApiKeyForAuthToken(authUser->getId(), auth.salt, auth.token) : std::string{};
                           },
                       },
                       authRequest)
        };

        // It is OK to have an empty authToken string here, as the auth service will reject and counts this as a bad attempt for this client address
        const auto authResult{ authTokenService.processAuthToken("subsonic", clientAddress, authToken) };

        switch (authResult.state)
        {
        case auth::IAuthTokenService::AuthTokenProcessResult::State::Granted:
            // Only the password mechanism needs this check as the token mechanism already used the user name
            if (const auto* passwordAuth{ std::get_if<PasswordAuthentication>(&authRequest) })
            {
                const auto authenticatedUser{ getUserFromUserId(session, authResult.authTokenInfo->userId) };
                if (!authenticatedUser || authenticatedUser->getLoginName() != passwordAuth->user)
                    throw WrongUsernameOrPasswordError{};
            }
            return authResult.authTokenInfo->userId;
        case auth::IAuthTokenService::AuthTokenProcessResult::State::Denied:
            if (std::holds_alternative<ApiKeyAuthentication>(authRequest))
                throw InvalidAPIkeyError{};
            else
                throw WrongUsernameOrPasswordError{};
        case auth::IAuthTokenService::AuthTokenProcessResult::State::Throttled:
            throw LoginThrottledGenericError{};
        }

        throw InternalErrorGenericError{ "Cannot authenticate user" };
    }
} // namespace lms::api::subsonic::utils
