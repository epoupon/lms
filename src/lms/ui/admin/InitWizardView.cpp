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

#include "InitWizardView.hpp"

#include <Wt/WComboBox.h>
#include <Wt/WFormModel.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>

#include "auth/IPasswordService.hpp"
#include "database/Session.hpp"
#include "database/User.hpp"
#include "utils/Exception.hpp"
#include "utils/Logger.hpp"
#include "utils/Service.hpp"

#include "common/Validators.hpp"
#include "common/AuthModeModel.hpp"
#include "LmsApplication.hpp"

namespace UserInterface {

class InitWizardModel : public Wt::WFormModel
{
	public:

		// Associate each field with a unique string literal.
		static const Field AdminLoginField;
		static const Field PasswordField;
		static const Field PasswordConfirmField;
		static inline const Field AuthModeField{"auth-mode"};

		InitWizardModel() : Wt::WFormModel()
		{
			addField(AdminLoginField);
			addField(AuthModeField);
			addField(PasswordField);
			addField(PasswordConfirmField);

			setValidator(AuthModeField, createMandatoryValidator());
			setValidator(AdminLoginField, createNameValidator());
			setValidator(PasswordField, createMandatoryValidator());
			setValidator(PasswordConfirmField, createMandatoryValidator());
		}

		std::shared_ptr<AuthModeModel> getAuthModeModel() const { return _authModeModel; }

		void saveData()
		{
			const Database::User::PasswordHash passwordHash {Service<::Auth::IPasswordService>::get()->hashPassword(valueText(PasswordField).toUTF8())};

			auto transaction(LmsApp->getDbSession().createUniqueTransaction());

			// Check if a user already exist
			// If it's the case, just do nothing
			if (!Database::User::getAll(LmsApp->getDbSession()).empty())
				throw LmsException("Admin user already created");

			auto authModeRow {_authModeModel->getRowFromString(valueText(AuthModeField))};
			if (!authModeRow)
				throw LmsException {"Bad authentication mode"};

			const Database::User::AuthMode authMode {_authModeModel->getValue(*authModeRow)};

			Database::User::pointer user {Database::User::create(LmsApp->getDbSession(), valueText(AdminLoginField).toUTF8())};
			user.modify()->setType(Database::User::Type::ADMIN);
			user.modify()->setAuthMode(authMode);
			if (authMode == Database::User::AuthMode::Internal)
				user.modify()->setPasswordHash(passwordHash);
		}

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
				// Evaluate the strength of the password
				if (!Service<::Auth::IPasswordService>::get()->evaluatePasswordStrength(valueText(AdminLoginField).toUTF8(), valueText(PasswordField).toUTF8()))
					error = Wt::WString::tr("Lms.password-too-weak");
			}
			else
				error = Wt::WString::tr("Lms.password-must-not-be-empty");
		}

		void validatePasswordConfirm(Wt::WString& error) const
		{
			auto authModeRow {_authModeModel->getRowFromString(valueText(AuthModeField))};
			if (!authModeRow)
				throw LmsException {"Bad authentication mode"};

			const Database::User::AuthMode authMode {_authModeModel->getValue(*authModeRow)};
			if (authMode != Database::User::AuthMode::Internal)
				return;

			if (validation(PasswordField).state() == Wt::ValidationState::Valid)
			{
				if (valueText(PasswordField) != valueText(PasswordConfirmField))
					error = Wt::WString::tr("Lms.passwords-dont-match");
			}
		}

		bool validateField(Field field)
		{
			Wt::WString error;

			if (field == PasswordField)
			{
				validatePassword(error);
			}
			else if (field == PasswordConfirmField)
			{
				validatePasswordConfirm(error);
			}
			else
			{
				return Wt::WFormModel::validateField(field);
			}

			setValidation(field, Wt::WValidator::Result( error.empty() ? Wt::ValidationState::Valid : Wt::ValidationState::Invalid, error));

			return (validation(field).state() == Wt::ValidationState::Valid);
		}

		std::shared_ptr<AuthModeModel>	_authModeModel {createAuthModeModel()};
};

const Wt::WFormModel::Field InitWizardModel::AdminLoginField = "admin-login";
const Wt::WFormModel::Field InitWizardModel::PasswordField = "password";
const Wt::WFormModel::Field InitWizardModel::PasswordConfirmField = "password-confirm";

InitWizardView::InitWizardView()
: Wt::WTemplateFormView(Wt::WString::tr("Lms.Admin.InitWizard.template"))
{
	auto model = std::make_shared<InitWizardModel>();

	// AdminLogin
	{
		auto adminLogin {std::make_unique<Wt::WLineEdit>()};
		adminLogin->setAttributeValue("autocomplete", "username");
		setFormWidget(InitWizardModel::AdminLoginField, std::move(adminLogin));
	}

	// Auth mode
	auto authMode = std::make_unique<Wt::WComboBox>();
	authMode->setModel(model->getAuthModeModel());
	authMode->activated().connect([=](int row)
	{
		const Database::User::AuthMode authMode {model->getAuthModeModel()->getValue(row)};

		model->setReadOnly(InitWizardModel::PasswordField, authMode != Database::User::AuthMode::Internal);
		model->setReadOnly(InitWizardModel::PasswordConfirmField, authMode != Database::User::AuthMode::Internal);
		updateModel(model.get());
		updateView(model.get());
	});
	setFormWidget(InitWizardModel::AuthModeField, std::move(authMode));

	// Password
	{
		auto passwordEdit = std::make_unique<Wt::WLineEdit>();
		passwordEdit->setEchoMode(Wt::EchoMode::Password);
		passwordEdit->setAttributeValue("autocomplete", "current-password");
		setFormWidget(InitWizardModel::PasswordField, std::move(passwordEdit) );
	}

	// Password confirmation
	{
		auto passwordConfirmEdit = std::make_unique<Wt::WLineEdit>();
		passwordConfirmEdit->setEchoMode(Wt::EchoMode::Password);
		passwordConfirmEdit->setAttributeValue("autocomplete", "current-password");
		setFormWidget(InitWizardModel::PasswordConfirmField, std::move(passwordConfirmEdit));
	}

	Wt::WPushButton* saveButton = bindNew<Wt::WPushButton>("create-btn", Wt::WString::tr("Lms.create"));
	saveButton->clicked().connect([=]
	{
		updateModel(model.get());

		if (model->validate())
		{
			model->saveData();
			LmsApp->notifyMsg(LmsApplication::MsgType::Success, Wt::WString::tr("Lms.Admin.InitWizard.done"));
			saveButton->setEnabled(false);
		}

		updateView(model.get());
	});

	updateView(model.get());
}

} // namespace UserInterface

