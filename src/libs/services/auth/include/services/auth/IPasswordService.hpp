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

#include <string_view>

#include <Wt/Dbo/ptr.h>
#include <Wt/WDateTime.h>
#include <boost/asio/ip/address.hpp>

#include "database/objects/UserId.hpp"
#include "services/auth/Types.hpp"

namespace lms::db
{
    class IDb;
    class User;
} // namespace lms::db

namespace lms::auth
{
    class IPasswordService
    {
    public:
        virtual ~IPasswordService() = default;

        struct CheckResult
        {
            enum class State
            {
                Granted,
                Denied,
                Throttled,
            };
            State state{ State::Denied };
            db::UserId userId{};
        };
        virtual CheckResult checkUserPassword(const boost::asio::ip::address& clientAddress,
            std::string_view loginName,
            std::string_view password)
            = 0;

        virtual bool canSetPasswords() const = 0;

        enum class PasswordAcceptabilityResult
        {
            OK,
            TooWeak,
            MustMatchLoginName,
        };
        virtual PasswordAcceptabilityResult checkPasswordAcceptability(std::string_view password, const PasswordValidationContext& context) const = 0;
        virtual void setPassword(db::UserId userId, std::string_view newPassword) = 0;
    };

    std::unique_ptr<IPasswordService> createPasswordService(std::string_view backend, db::IDb& db, std::size_t maxThrottlerEntryCount);
} // namespace lms::auth
