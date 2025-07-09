/*
 * Copyright (C) 2021 Emeric Poupon
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

#include <Wt/Auth/HashFunction.h>
#include <Wt/Auth/PasswordStrengthValidator.h>

#include "database/objects/User.hpp"

#include "PasswordServiceBase.hpp"
#include "services/auth/IPasswordService.hpp"

namespace lms::auth
{
    class InternalPasswordService : public PasswordServiceBase
    {
    public:
        InternalPasswordService(db::IDb& db, std::size_t maxThrottlerEntries);

    private:
        bool checkUserPassword(std::string_view loginName, std::string_view password) override;

        bool canSetPasswords() const override;
        PasswordAcceptabilityResult checkPasswordAcceptability(std::string_view password, const PasswordValidationContext& context) const override;
        void setPassword(db::UserId userId, std::string_view newPassword) override;

        db::User::PasswordHash hashPassword(std::string_view password) const;
        void hashRandomPassword() const;

        const unsigned _bcryptRoundCount;
        const Wt::Auth::BCryptHashFunction _hashFunc{ static_cast<int>(_bcryptRoundCount) };
        Wt::Auth::PasswordStrengthValidator _validator;
    };
} // namespace lms::auth
