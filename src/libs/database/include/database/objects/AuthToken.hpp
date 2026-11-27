/*
 * Copyright (C) 2013 Emeric Poupon
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
#include <span>
#include <string_view>

#include <Wt/WDateTime.h>

#include "database/objects/AuthTokenId.hpp"
#include "database/objects/UserId.hpp"
#include "database/objects/detail/Object.hpp"

namespace lms::db
{
    class ISession;

    class AuthToken : public detail::Object<AuthToken, AuthTokenId>
    {
    public:
        static AuthToken create(ISession& session, std::string_view domain, std::string_view value, const Wt::WDateTime& expiry, std::optional<long> maxUseCount, UserId user);
        static void destroy(ISession& session, std::span<const AuthTokenId> ids);

        // Utility
        static std::size_t getCount(ISession& session);
        static std::optional<AuthToken> find(ISession& session, AuthTokenId tokenId);
        static std::optional<AuthToken> find(ISession& session, std::string_view domain, std::string_view value);
        static void find(ISession& session, std::string_view domain, UserId userId, const std::function<void(const AuthToken&)>& visitor);
        static void removeExpiredTokens(ISession& session, std::string_view domain, const Wt::WDateTime& now);
        static void clearUserTokens(ISession& session, std::string_view domain, UserId user);

        static void update(ISession& session, const AuthToken& authToken);

        // Accessors
        const Wt::WDateTime& getExpiry() const { return _expiry; }
        UserId getUserId() const { return _userId; }
        const std::string& getValue() const { return _value; }
        std::size_t getUseCount() const { return _useCount; }
        Wt::WDateTime getLastUsed() const { return _lastUsed; }
        std::optional<std::size_t> getMaxUseCount() const { return _maxUseCount; }

        // Setters
        std::size_t incUseCount() { return ++_useCount; }
        void setLastUsed(const Wt::WDateTime& lastUsed) { _lastUsed = lastUsed; }

    protected:
        AuthToken() = default;
        AuthToken(std::string_view domain, std::string_view value, const Wt::WDateTime& expiry, std::optional<long> maxUseCount);

        std::string _domain;
        std::string _value;
        Wt::WDateTime _expiry;
        long _useCount{};
        Wt::WDateTime _lastUsed;
        std::optional<long> _maxUseCount;
        UserId _userId;
    };
} // namespace lms::db