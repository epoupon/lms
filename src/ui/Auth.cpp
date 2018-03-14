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

#include <Wt/WFormModel>
#include <Wt/WLineEdit>
#include <Wt/WCheckBox>
#include <Wt/WPushButton>

#include "utils/Logger.hpp"

#include "common/Validators.hpp"
#include "LmsApplication.hpp"

#include "Auth.hpp"

namespace UserInterface {

Auth::Auth(Wt::WContainerWidget *parent)
: Wt::WTemplateFormView(parent)
{
	_model = new Wt::Auth::AuthModel(DbHandler().getAuthService(), DbHandler().getUserDatabase());

	_model->addPasswordAuth(&Database::Handler::getPasswordService());

	setTemplateText(Wt::WString::tr("Lms.Auth.template"));
	addFunction("tr", &WTemplate::Functions::tr);
	addFunction("id", &WTemplate::Functions::id);

	// LoginName
	auto loginName = new Wt::WLineEdit();
	setFormWidget(Wt::Auth::AuthModel::LoginNameField, loginName);

	// Password
	auto password = new Wt::WLineEdit();
	setFormWidget(Wt::Auth::AuthModel::PasswordField, password);
	password->setEchoMode(Wt::WLineEdit::Password);

	// Remember Me
	auto rememberMe = new Wt::WCheckBox();
	setFormWidget(Wt::Auth::AuthModel::RememberMeField, rememberMe);

	auto loginBtn = new Wt::WPushButton(Wt::WString::tr("Lms.login"));
	bindWidget("login-btn", loginBtn);
	loginBtn->clicked().connect(std::bind([=]
	{
		updateModel(_model);

		if (_model->validate())
			_model->login(DbHandler().getLogin());
		else
			updateView(_model);
	}));

	password->enterPressed().connect(std::bind([=]
	{
		loginBtn->clicked().emit(Wt::WMouseEvent());
	}));

	DbHandler().getLogin().changed().connect(std::bind([=]
	{
		if (DbHandler().getLogin().loggedIn())
			this->setHidden(true);
	}));

	Wt::Auth::User user = _model->processAuthToken();
	_model->loginUser(DbHandler().getLogin(), user, Wt::Auth::WeakLogin);

	updateView(_model);
}

void
Auth::logout()
{
	_model->logout(DbHandler().getLogin());
}

} // namespace UserInterface

