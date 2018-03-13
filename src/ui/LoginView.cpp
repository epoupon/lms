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
#include <Wt/Auth/AuthModel>

#include "utils/Logger.hpp"

#include "common/Validators.hpp"
#include "LmsApplication.hpp"

#include "LoginView.hpp"

namespace UserInterface {

LoginView::LoginView(Wt::Auth::Login& login, Wt::WContainerWidget *parent)
: Wt::WTemplateFormView(parent)
{
	auto model = new Wt::Auth::AuthModel(DbHandler().getAuthService(), DbHandler().getUserDatabase());

	model->addPasswordAuth(&Database::Handler::getPasswordService());

	setTemplateText(Wt::WString::tr("template-login"));
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

	auto loginBtn = new Wt::WPushButton(Wt::WString::tr("msg-login"));
	bindWidget("login-btn", loginBtn);
	loginBtn->clicked().connect(std::bind([=, &login]
	{
		updateModel(model);

		if (model->validate())
			model->login(login);
		else
			updateView(model);
	}));

	password->enterPressed().connect(std::bind([=]
	{
		loginBtn->clicked().emit(Wt::WMouseEvent());
	}));

	login.changed().connect(std::bind([=, &login]
	{
		if (login.loggedIn())
			this->setHidden(true);
	}));

	Wt::Auth::User user = model->processAuthToken();
	model->loginUser(login, user, Wt::Auth::WeakLogin);

	updateView(model);
}

} // namespace UserInterface

