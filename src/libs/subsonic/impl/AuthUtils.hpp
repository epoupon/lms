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

#pragma once

#include <string>
#include <string_view>
#include <variant>

#include <Wt/Http/Request.h>

#include "database/objects/User.hpp"
#include "database/objects/UserId.hpp"

namespace lms::db
{
    class Session;
}

namespace lms::api::subsonic::utils
{
    struct PasswordAuthentication
    {
        std::string user;
        std::string password;
    };

    struct TokenAuthentication
    {
        std::string user;
        std::string token;
        std::string salt;
    };

    struct ApiKeyAuthentication
    {
        std::string apiKey;
    };

    using AuthenticationRequest = std::variant<PasswordAuthentication, TokenAuthentication, ApiKeyAuthentication>;

    // Throws on error
    AuthenticationRequest parseAndValidateAuthenticationRequest(const Wt::Http::ParameterMap& parameters);

    // Checks token == hex(md5(apiKey + salt)), case insensitive
    bool checkAuthToken(std::string_view apiKey, std::string_view salt, std::string_view token);

    // Throws UserNotAuthorizedError if no such user exists
    db::User::pointer getUserFromUserId(db::Session& session, db::UserId userId);

    // Throws on error
    db::UserId authenticateUser(const Wt::Http::Request& request, db::Session& session);
} // namespace lms::api::subsonic::utils
