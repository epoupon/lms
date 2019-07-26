/*
 * Copyright (C) 2018 Emeric Poupon
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

#include "Auth.hpp"

#include <iomanip>

#include <Wt/WFormModel.h>
#include <Wt/WLineEdit.h>
#include <Wt/WCheckBox.h>
#include <Wt/WPushButton.h>

#include "auth/AuthService.hpp"
#include "main/Service.hpp"
#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

#include "common/Validators.hpp"
#include "LmsApplication.hpp"

namespace UserInterface {

static const std::string authCookieName {"LmsAuth"};

static
std::string
createSecret()
{
	std::array<std::uint8_t, 32> buffer;
	fillRandom(buffer);

	std::ostringstream oss;
	for (std::uint8_t b : buffer)
		oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(b);

	return oss.str();
}


static
void
createAuthToken(Database::IdType userId)
{

	const std::string secret {createSecret()};
	const Wt::WDateTime now {Wt::WDateTime::currentDateTime()};
	const Wt::WDateTime expiry {now.addYears(1)};

	{
		auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

		Database::User::pointer user {Database::User::getById(LmsApp->getDbSession(), userId)};
		Database::AuthToken::create(LmsApp->getDbSession(), secret, expiry, user);

		LMS_LOG(UI, DEBUG) << "Created auth token for user '" << user->getLoginName() << "', expiry = " << expiry.toString();
	}

	LmsApp->setCookie(authCookieName,
			secret,
			expiry.toTime_t() - now.toTime_t(),
			"",
			"",
			LmsApp->environment().urlScheme() == "https");
}


boost::optional<Database::IdType>
processAuthToken(const Wt::WEnvironment& env)
{
	const std::string* authCookie {env.getCookie(authCookieName)};
	if (!authCookie)
		return boost::none;

	Database::IdType userId {};
	{
		auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

		Database::AuthToken::pointer authToken {Database::AuthToken::getByValue(LmsApp->getDbSession(), *authCookie)};
		if (!authToken)
		{
			LMS_LOG(UI, INFO) << "Client '" << env.clientAddress() << "' presented a token that has not been found";
			return boost::none;
		}

		if (authToken->getExpiry() < Wt::WDateTime::currentDateTime())
		{
			LMS_LOG(UI, INFO) << "Expired auth token for user '" << authToken->getUser()->getLoginName() << "'!";
			authToken.remove();
			return boost::none;
		}

		LMS_LOG(UI, DEBUG) << "Found auth token for user '" << authToken->getUser()->getLoginName() << "'!";
		userId = authToken->getUser().id();

		authToken.remove();
	}

	createAuthToken(userId);

	return userId;
}

class AuthModel : public Wt::WFormModel
{
	public:

		// Associate each field with a unique string literal.
		static const Field LoginNameField;
		static const Field PasswordField;
		static const Field RememberMeField;

		AuthModel()
		{
			addField(LoginNameField);
			addField(PasswordField);
			addField(RememberMeField);

			setValidator(LoginNameField, createNameValidator());
			setValidator(PasswordField, createMandatoryValidator());
		}


		void saveData()
		{
			{
				auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

				Database::User::pointer user {Database::User::getByLoginName(LmsApp->getDbSession(), valueText(LoginNameField).toUTF8())};
				user.modify()->setLastLogin(Wt::WDateTime::currentDateTime());
				_userId = user.id();
			}

			if (Wt::asNumber(value(RememberMeField)))
				createAuthToken(*_userId);
		}

		bool validateField(Field field)
		{
			Wt::WString error;

			if (field == PasswordField)
			{
				switch (getService<::Auth::AuthService>()->checkUserPassword(
							LmsApp->getDbSession(),
							boost::asio::ip::address::from_string(LmsApp->environment().clientAddress()),
							valueText(LoginNameField).toUTF8(),
							valueText(PasswordField).toUTF8()))
				{
					case ::Auth::AuthService::PasswordCheckResult::Match:
						break;
					case ::Auth::AuthService::PasswordCheckResult::Mismatch:
						error = Wt::WString::tr("Lms.password-bad-login-combination");
						break;
					case ::Auth::AuthService::PasswordCheckResult::Throttled:
						error = Wt::WString::tr("Lms.password-client-throttled");
						break;
				}
			}
			else
			{
				return Wt::WFormModel::validateField(field);
			}

			setValidation(field, Wt::WValidator::Result( error.empty() ? Wt::ValidationState::Valid : Wt::ValidationState::Invalid, error));

			return (validation(field).state() == Wt::ValidationState::Valid);
		}

		boost::optional<Database::IdType> getUserId() const { return _userId; }

	private:

		boost::optional<Database::IdType> _userId;
};

const AuthModel::Field AuthModel::LoginNameField {"login-name"};
const AuthModel::Field AuthModel::PasswordField {"password"};
const AuthModel::Field AuthModel::RememberMeField {"remember-me"};


Auth::Auth()
: Wt::WTemplateFormView {Wt::WString::tr("Lms.Auth.template")}
{
	auto model {std::make_shared<AuthModel>()};

	auto processAuth = [=]()
	{
		updateModel(model.get());

		if (model->validate())
		{
			model->saveData();
			userLoggedIn.emit(*model->getUserId());
		}
		else
			updateView(model.get());
	};

	// LoginName
	setFormWidget(AuthModel::LoginNameField, std::make_unique<Wt::WLineEdit>());

	// Password
	auto password = std::make_unique<Wt::WLineEdit>();
	password->setEchoMode(Wt::EchoMode::Password);
	password->enterPressed().connect(this, processAuth);
	setFormWidget(AuthModel::PasswordField, std::move(password));

	// Remember me
	setFormWidget(AuthModel::RememberMeField, std::make_unique<Wt::WCheckBox>());

	{
		auto transaction {LmsApp->getDbSession().createSharedTransaction()};

		Database::User::pointer demoUser {Database::User::getDemo(LmsApp->getDbSession())};
		if (demoUser)
		{
			model->setValue(AuthModel::LoginNameField, demoUser->getLoginName());
			model->setValue(AuthModel::PasswordField, demoUser->getLoginName());
		}
	}

	Wt::WPushButton* loginBtn {bindNew<Wt::WPushButton>("login-btn", Wt::WString::tr("Lms.login"))};
	loginBtn->clicked().connect(this, processAuth);

	updateView(model.get());

}

} // namespace UserInterface

