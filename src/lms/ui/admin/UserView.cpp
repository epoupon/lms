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

#include "UserView.hpp"

#include <Wt/WCheckBox.h>
#include <Wt/WComboBox.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WTemplateFormView.h>

#include <Wt/WFormModel.h>

#include "auth/IPasswordService.hpp"
#include "database/User.hpp"
#include "database/Session.hpp"
#include "utils/IConfig.hpp"
#include "utils/Exception.hpp"
#include "utils/Logger.hpp"
#include "utils/Service.hpp"
#include "utils/String.hpp"

#include "common/AuthModeModel.hpp"
#include "common/Validators.hpp"
#include "common/ValueStringModel.hpp"
#include "LmsApplication.hpp"
#include "LmsApplicationException.hpp"

namespace UserInterface {

using namespace Database;

class UserModel : public Wt::WFormModel
{

	public:
		static inline const Field LoginField {"login"};
		static inline const Field PasswordField {"password"};
		static inline const Field DemoField {"demo"};
		static inline const Field AuthModeField{"auth-mode"};

		using AuthModeModel = ValueStringModel<User::AuthMode>;

		UserModel(std::optional<Database::IdType> userId)
		: _userId {userId}
		{
			if (!_userId)
			{
				addField(LoginField);
				setValidator(LoginField, createNameValidator());
			}

			addField(AuthModeField);
			addField(PasswordField);
			addField(DemoField);

			setValidator(AuthModeField, createMandatoryValidator());

			loadData();
		}

		std::shared_ptr<AuthModeModel> getAuthModeModel() const { return _authModeModel; }

		void saveData()
		{
			std::optional<Database::User::PasswordHash> passwordHash;
			if (!valueText(PasswordField).empty())
				passwordHash = Service<::Auth::IPasswordService>::get()->hashPassword(valueText(PasswordField).toUTF8());

			auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

			if (_userId)
			{
				// Update user
				Database::User::pointer user {Database::User::getById(LmsApp->getDbSession(), *_userId)};

				auto authModeRow {_authModeModel->getRowFromString(valueText(AuthModeField))};
				if (!authModeRow)
					throw LmsException {"Bad authentication mode"};

				const Database::User::AuthMode authMode {_authModeModel->getValue(*authModeRow)};
				user.modify()->setAuthMode(authMode);
				if (authMode == Database::User::AuthMode::Internal && passwordHash)
				{
					user.modify()->setPasswordHash(*passwordHash);
					user.modify()->clearAuthTokens();
				}
			}
			else
			{
				// Create user
				Database::User::pointer user {Database::User::create(LmsApp->getDbSession(), valueText(LoginField).toUTF8())};

				if (Wt::asNumber(value(DemoField)))
					user.modify()->setType(Database::User::Type::DEMO);

				auto authModeRow {_authModeModel->getRowFromString(valueText(AuthModeField))};
				if (!authModeRow)
					throw LmsException {"Bad authentication mode"};

				const Database::User::AuthMode authMode {_authModeModel->getValue(*authModeRow)};
				user.modify()->setAuthMode(authMode);
				if (authMode == Database::User::AuthMode::Internal)
					user.modify()->setPasswordHash(*passwordHash);
			}
		}

	private:

		void validatePassword(Wt::WString& error) const
		{
			auto authModeRow {_authModeModel->getRowFromString(valueText(AuthModeField))};
			if (!authModeRow)
				throw LmsException {"Bad authentication mode"};

			const Database::User::AuthMode authMode {_authModeModel->getValue(*authModeRow)};
			if (authMode != Database::User::AuthMode::Internal)
				return;

			if (!valueText(PasswordField).empty())
			{
				if (Wt::asNumber(value(DemoField)))
				{
					// Demo account: password must be the same as the login name
					if (valueText(PasswordField) != getLoginName())
						error = Wt::WString::tr("Lms.Admin.User.demo-password-invalid");
				}
				else
				{
					// Evaluate the strength of the password for non demo accounts
					if (!Service<::Auth::IPasswordService>::get()->evaluatePasswordStrength(getLoginName(), valueText(PasswordField).toUTF8()))
						error = Wt::WString::tr("Lms.password-too-weak");
				}
			}
			else
			{
				auto transaction {LmsApp->getDbSession().createSharedTransaction()};

				bool needPassword {true};

				// Allow an empty password if and only if the user previously had one set
				if (_userId)
				{
					const Database::User::pointer user {Database::User::getById(LmsApp->getDbSession(), *_userId)};
					if (!user)
						throw UserNotFoundException {*_userId};

					needPassword = user->getPasswordHash().hash.empty();
				}

				if (needPassword)
					error = Wt::WString::tr("Lms.password-must-not-be-empty");
			}
		}

		void loadData()
		{
			if (!_userId)
				return;

			auto transaction {LmsApp->getDbSession().createSharedTransaction()};

			const Database::User::pointer user {Database::User::getById(LmsApp->getDbSession(), *_userId)};
			if (!user)
				throw UserNotFoundException {*_userId};
			else if (user == LmsApp->getUser())
				throw UserNotAllowedException {};

			auto authModeRow {_authModeModel->getRowFromValue(user->getAuthMode())};
			if (authModeRow)
			{
				setValue(AuthModeField, _authModeModel->getString(*authModeRow));
				if (_authModeModel->getValue(*authModeRow) != User::AuthMode::Internal)
					setReadOnly(PasswordField, true);
			}
		}

		std::string getLoginName() const
		{
			if (_userId)
			{
				auto transaction {LmsApp->getDbSession().createSharedTransaction()};

				const Database::User::pointer user {Database::User::getById(LmsApp->getDbSession(), *_userId)};
				return user->getLoginName();
			}
			else
				return valueText(LoginField).toUTF8();
		}

		bool validateField(Field field)
		{
			Wt::WString error;

			if (field == LoginField)
			{
				auto transaction {LmsApp->getDbSession().createSharedTransaction()};

				const Database::User::pointer user {Database::User::getByLoginName(LmsApp->getDbSession(), valueText(LoginField).toUTF8())};
				if (user)
					error = Wt::WString::tr("Lms.Admin.User.user-already-exists");
			}
			else if (field == PasswordField)
			{
				validatePassword(error);
			}
			else if (field == DemoField)
			{
				auto transaction {LmsApp->getDbSession().createSharedTransaction()};

				if (Wt::asNumber(value(DemoField)) && Database::User::getDemo(LmsApp->getDbSession()))
					error = Wt::WString::tr("Lms.Admin.User.demo-account-already-exists");
			}

			if (error.empty())
				return Wt::WFormModel::validateField(field);

			setValidation(field, Wt::WValidator::Result( Wt::ValidationState::Invalid, error));

			return false;
		}

		std::optional<Database::IdType> _userId;
		std::shared_ptr<AuthModeModel>	_authModeModel {createAuthModeModel()};
};

UserView::UserView()
{
	wApp->internalPathChanged().connect(this, [this]()
	{
		refreshView();
	});

	refreshView();
}

void
UserView::refreshView()
{
	if (!wApp->internalPathMatches("/admin/user"))
		return;

	auto userId = StringUtils::readAs<Database::IdType>(wApp->internalPathNextPart("/admin/user/"));

	clear();

	Wt::WTemplateFormView* t {addNew<Wt::WTemplateFormView>(Wt::WString::tr("Lms.Admin.User.template"))};

	auto model {std::make_shared<UserModel>(userId)};

	if (userId)
	{
		auto transaction {LmsApp->getDbSession().createSharedTransaction()};

		const Database::User::pointer user {Database::User::getById(LmsApp->getDbSession(), *userId)};
		if (!user)
			throw UserNotFoundException {*userId};

		t->bindString("title", Wt::WString::tr("Lms.Admin.User.user-edit").arg(user->getLoginName()), Wt::TextFormat::Plain);
		t->setCondition("if-has-last-login", true);
		t->bindString("last-login", user->getLastLogin().toString(), Wt::TextFormat::Plain);
	}
	else
	{
		// Login
		t->setCondition("if-has-login", true);
		t->setFormWidget(UserModel::LoginField, std::make_unique<Wt::WLineEdit>());
		t->bindString("title", Wt::WString::tr("Lms.Admin.User.user-create"));
	}

	// Auth mode
	auto authMode = std::make_unique<Wt::WComboBox>();
	authMode->setModel(model->getAuthModeModel());
	authMode->activated().connect([=](int row)
	{
		const User::AuthMode authMode {model->getAuthModeModel()->getValue(row)};

		model->setReadOnly(UserModel::PasswordField, authMode != User::AuthMode::Internal);
		t->updateModel(model.get());
		t->updateView(model.get());
	});
	t->setFormWidget(UserModel::AuthModeField, std::move(authMode));

	// Password
	auto passwordEdit = std::make_unique<Wt::WLineEdit>();
	passwordEdit->setEchoMode(Wt::EchoMode::Password);
	passwordEdit->setAttributeValue("autocomplete", "off");
	t->setFormWidget(UserModel::PasswordField, std::move(passwordEdit));

	// Demo account
	t->setFormWidget(UserModel::DemoField, std::make_unique<Wt::WCheckBox>());
	if (!userId && Service<IConfig>::get()->getBool("demo", false))
		t->setCondition("if-demo", true);

	Wt::WPushButton* saveBtn = t->bindNew<Wt::WPushButton>("save-btn", Wt::WString::tr(userId ? "Lms.save" : "Lms.create"));
	saveBtn->clicked().connect([=]()
	{
		t->updateModel(model.get());

		if (model->validate())
		{
			model->saveData();
			LmsApp->notifyMsg(MsgType::Success, Wt::WString::tr(userId ? "Lms.Admin.User.user-updated" : "Lms.Admin.User.user-created"));
			LmsApp->setInternalPath("/admin/users", true);
		}
		else
		{
			t->updateView(model.get());
		}
	});

	t->updateView(model.get());
}

} // namespace UserInterface


