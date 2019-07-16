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

#include <Wt/WFormModel.h>
#include <Wt/WLineEdit.h>
#include <Wt/WCheckBox.h>
#include <Wt/WPushButton.h>

#include "utils/Logger.hpp"

#include "common/Validators.hpp"
#include "LmsApplication.hpp"

namespace UserInterface {

Auth::Auth()
: Wt::WTemplateFormView(Wt::WString::tr("Lms.Auth.template"))
{
	_model = std::make_shared<Wt::Auth::AuthModel>(LmsApp->getDbSession().getAuthService(), LmsApp->getDbSession().getUserDatabase());
	_model->addPasswordAuth(&Database::Session::getPasswordService());

	// LoginName
	setFormWidget(Wt::Auth::AuthModel::LoginNameField, std::make_unique<Wt::WLineEdit>());

	{
		auto transaction {LmsApp->getDbSession().createSharedTransaction()};

		auto demoUser = Database::User::getDemo(LmsApp->getDbSession());
		if (demoUser)
		{
			const std::string userName {LmsApp->getDbSession().getUserLoginName(demoUser)};
			_model->setValue(Wt::Auth::AuthModel::LoginNameField, userName );
			_model->setValue(Wt::Auth::AuthModel::PasswordField, userName);
		}
	}

	// Password
	auto password = std::make_unique<Wt::WLineEdit>();
	password->setEchoMode(Wt::EchoMode::Password);
	password->enterPressed().connect(this, &Auth::processAuth);
	setFormWidget(Wt::Auth::AuthModel::PasswordField, std::move(password));

	// Remember Me
	setFormWidget(Wt::Auth::AuthModel::RememberMeField, std::make_unique<Wt::WCheckBox>());

	Wt::WPushButton* loginBtn = bindNew<Wt::WPushButton>("login-btn", Wt::WString::tr("Lms.login"));
	loginBtn->clicked().connect(this, &Auth::processAuth);

	LmsApp->getDbSession().getLogin().changed().connect(std::bind([=]
	{
		if (LmsApp->getDbSession().getLogin().loggedIn())
			this->setHidden(true);
	}));

	updateView(_model.get());

	Wt::Auth::User user = _model->processAuthToken();
	if (user.isValid())
	{
		LMS_LOG(UI, DEBUG) << "Valid user found from auth token (id = " << user.id() << ")";
		_model->loginUser(LmsApp->getDbSession().getLogin(), user, Wt::Auth::LoginState::Weak);
	}
}

void
Auth::processAuth()
{
	updateModel(_model.get());

	if (_model->validate())
		_model->login(LmsApp->getDbSession().getLogin());
	else
		updateView(_model.get());
}

void
Auth::logout()
{
	_model->logout(LmsApp->getDbSession().getLogin());
}

} // namespace UserInterface

