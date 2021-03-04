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

#include "auth/IPasswordService.hpp"
#include "utils/Service.hpp"
#include "LmsApplication.hpp"

namespace UserInterface
{
	class PasswordStrengthValidator : public Wt::WValidator
	{
		public:
			PasswordStrengthValidator(LoginNameGetFunc loginNameGetFunc) : _loginNameGetFunc {std::move(loginNameGetFunc)} {}

			Wt::WValidator::Result validate(const Wt::WString& input) const override;

		private:
			LoginNameGetFunc _loginNameGetFunc;
	};

	Wt::WValidator::Result
	PasswordStrengthValidator::validate(const Wt::WString& input) const
	{
		if (input.empty())
			return Wt::WValidator::validate(input);

		if (Service<::Auth::IPasswordService>::get()->isPasswordSecureEnough(_loginNameGetFunc(), input.toUTF8()))
			return Wt::WValidator::Result {Wt::ValidationState::Valid};

		return Wt::WValidator::Result {Wt::ValidationState::Invalid, Wt::WString::tr("Lms.password-too-weak")};
	}

	std::shared_ptr<Wt::WValidator>
	createPasswordStrengthValidator(std::string_view loginName)
	{
		return std::make_shared<PasswordStrengthValidator>([loginName = std::string {loginName}] { return loginName; });
	}

	std::shared_ptr<Wt::WValidator> createPasswordStrengthValidator(LoginNameGetFunc loginNameGetFunc)
	{
		return std::make_shared<PasswordStrengthValidator>(std::move(loginNameGetFunc));
	}

	class PasswordCheckValidator : public Wt::WValidator
	{
		public:
			Wt::WValidator::Result validate(const Wt::WString& input) const override;
	};

	Wt::WValidator::Result
	PasswordCheckValidator::validate(const Wt::WString& input) const
	{
		const auto checkResult {Service<::Auth::IPasswordService>::get()->checkUserPassword(
					LmsApp->getDbSession(),
					boost::asio::ip::address::from_string(LmsApp->environment().clientAddress()),
					LmsApp->getUserLoginName(),
					input.toUTF8())};
		switch (checkResult.state)
		{
			case ::Auth::IPasswordService::CheckResult::State::Granted:
				return Wt::WValidator::Result {Wt::ValidationState::Valid};
			case ::Auth::IPasswordService::CheckResult::State::Denied:
				return Wt::WValidator::Result {Wt::ValidationState::Invalid, Wt::WString::tr("Lms.Settings.password-bad")};
			case ::Auth::IPasswordService::CheckResult::State::Throttled:
				return Wt::WValidator::Result {Wt::ValidationState::Invalid, Wt::WString::tr("Lms.password-client-throttled")};
		}

		throw LmsException {"InternalError"};
	}

	std::shared_ptr<Wt::WValidator>
	createPasswordCheckValidator()
	{
		return std::make_shared<PasswordCheckValidator>();
	}

} // namespace UserInterface

