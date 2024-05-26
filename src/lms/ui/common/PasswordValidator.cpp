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

#include "PasswordValidator.hpp"

#include <Wt/WEnvironment.h>

#include "core/Service.hpp"
#include "services/auth/IPasswordService.hpp"

#include "LmsApplication.hpp"

namespace lms::ui
{
    namespace
    {
        class PasswordStrengthValidator : public Wt::WValidator
        {
        public:
            PasswordStrengthValidator(PasswordValidationContextGetFunc passwordValidationContextGetFunc)
                : _passwordValidationContextGetFunc{ std::move(passwordValidationContextGetFunc) }
            {
            }

        private:
            Wt::WValidator::Result validate(const Wt::WString& input) const override;
            std::string javaScriptValidate() const override { return {}; }

            PasswordValidationContextGetFunc _passwordValidationContextGetFunc;
        };
    } // namespace

    Wt::WValidator::Result PasswordStrengthValidator::validate(const Wt::WString& input) const
    {
        if (input.empty())
            return Wt::WValidator::validate(input);

        const auth::PasswordValidationContext context{ _passwordValidationContextGetFunc() };

        switch (core::Service<auth::IPasswordService>::get()->checkPasswordAcceptability(input.toUTF8(), context))
        {
        case auth::IPasswordService::PasswordAcceptabilityResult::OK:
            return Wt::WValidator::Result{ Wt::ValidationState::Valid };
        case auth::IPasswordService::PasswordAcceptabilityResult::TooWeak:
            return Wt::WValidator::Result{ Wt::ValidationState::Invalid, Wt::WString::tr("Lms.password-too-weak") };
        case auth::IPasswordService::PasswordAcceptabilityResult::MustMatchLoginName:
            return Wt::WValidator::Result{ Wt::ValidationState::Invalid, Wt::WString::tr("Lms.password-must-match-login") };
        }

        throw core::LmsException{ "internal error" };
    }

    std::unique_ptr<Wt::WValidator> createPasswordStrengthValidator(PasswordValidationContextGetFunc passwordValidationContextGetFunc)
    {
        return std::make_unique<PasswordStrengthValidator>(std::move(passwordValidationContextGetFunc));
    }

    class PasswordCheckValidator : public Wt::WValidator
    {
    private:
        Wt::WValidator::Result validate(const Wt::WString& input) const override;
        std::string javaScriptValidate() const override { return {}; }
    };

    Wt::WValidator::Result PasswordCheckValidator::validate(const Wt::WString& input) const
    {
        if (input.empty())
            return Wt::WValidator::validate(input);

        const auto checkResult{ core::Service<auth::IPasswordService>::get()->checkUserPassword(
            boost::asio::ip::address::from_string(LmsApp->environment().clientAddress()),
            LmsApp->getUserLoginName(),
            input.toUTF8()) };
        switch (checkResult.state)
        {
        case auth::IPasswordService::CheckResult::State::Granted:
            return Wt::WValidator::Result{ Wt::ValidationState::Valid };
        case auth::IPasswordService::CheckResult::State::Denied:
            return Wt::WValidator::Result{ Wt::ValidationState::Invalid, Wt::WString::tr("Lms.Settings.password-bad") };
        case auth::IPasswordService::CheckResult::State::Throttled:
            return Wt::WValidator::Result{ Wt::ValidationState::Invalid, Wt::WString::tr("Lms.password-client-throttled") };
        }

        throw core::LmsException{ "InternalError" };
    }

    std::unique_ptr<Wt::WValidator> createPasswordCheckValidator()
    {
        return std::make_unique<PasswordCheckValidator>();
    }

} // namespace lms::ui