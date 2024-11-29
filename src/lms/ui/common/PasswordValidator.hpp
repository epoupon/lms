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

#include <functional>

#include <Wt/WValidator.h>

#include "services/auth/Types.hpp"

namespace lms::auth
{
    class IPasswordService;
}

namespace lms::ui
{
    using PasswordValidationContextGetFunc = std::function<auth::PasswordValidationContext()>;
    std::unique_ptr<Wt::WValidator> createPasswordStrengthValidator(const auth::IPasswordService& passwordService, PasswordValidationContextGetFunc passwordValidationContextGetFunc);

    // Check current user password
    std::unique_ptr<Wt::WValidator> createPasswordCheckValidator(auth::IPasswordService& passwordService);
} // namespace lms::ui
