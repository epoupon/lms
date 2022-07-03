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

#include "services/auth/IPasswordService.hpp"
#include "services/database/Session.hpp"
#include "services/database/User.hpp"
#include "utils/Exception.hpp"
#include "utils/Logger.hpp"
#include "utils/Service.hpp"

#include "common/LoginNameValidator.hpp"
#include "common/MandatoryValidator.hpp"
#include "common/PasswordValidator.hpp"
#include "LmsApplication.hpp"

namespace UserInterface {

class InitWizardModel : public Wt::WFormModel
{
	public:
		// Associate each field with a unique string literal.
		static inline const Field AdminLoginField {"admin-login"};
		static inline const Field PasswordField {"password"};
		static inline const Field PasswordConfirmField {"password-confirm"};

		InitWizardModel() : Wt::WFormModel()
		{
			addField(AdminLoginField);
			addField(PasswordField);
			addField(PasswordConfirmField);

			setValidator(AdminLoginField, createLoginNameValidator());
			setValidator(PasswordField, createPasswordStrengthValidator([this] { return ::Auth::PasswordValidationContext {valueText(AdminLoginField).toUTF8(), Database::UserType::ADMIN}; }));
			validator(PasswordField)->setMandatory(true);
			setValidator(PasswordConfirmField, createMandatoryValidator());
		}

		void saveData()
		{
			auto transaction(LmsApp->getDbSession().createUniqueTransaction());

			// Check if a user already exist
			// If it's the case, just do nothing
			if (Database::User::getCount(LmsApp->getDbSession()) > 0)
				throw LmsException {"Admin user already created"};

			Database::User::pointer user {LmsApp->getDbSession().create<Database::User>(valueText(AdminLoginField).toUTF8())};
			user.modify()->setType(Database::UserType::ADMIN);
			Service<::Auth::IPasswordService>::get()->setPassword(user->getId(), valueText(PasswordField).toUTF8());
		}

		bool validateField(Field field)
		{
			Wt::WString error;

			if (field == PasswordConfirmField)
			{
				if (validation(PasswordField).state() == Wt::ValidationState::Valid
						&& valueText(PasswordField) != valueText(PasswordConfirmField))
				{
					error = Wt::WString::tr("Lms.passwords-dont-match");
				}
				else
					return Wt::WFormModel::validateField(field);
			}
			else
			{
				return Wt::WFormModel::validateField(field);
			}

			setValidation(field, Wt::WValidator::Result {Wt::ValidationState::Invalid, error});

			return false;
		}
};

InitWizardView::InitWizardView()
: Wt::WTemplateFormView {Wt::WString::tr("Lms.Admin.InitWizard.template")}
{
	auto model = std::make_shared<InitWizardModel>();

	// AdminLogin
	{
		auto adminLogin {std::make_unique<Wt::WLineEdit>()};
		adminLogin->setAttributeValue("autocomplete", "username");
		setFormWidget(InitWizardModel::AdminLoginField, std::move(adminLogin));
	}

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

	// Result notification
	Wt::WText* resultNotification {bindNew<Wt::WText>("info")};
	resultNotification->setHidden(true);


	Wt::WPushButton* saveButton = bindNew<Wt::WPushButton>("create-btn", Wt::WString::tr("Lms.create"));
	saveButton->clicked().connect([=]
	{
		updateModel(model.get());

		if (model->validate())
		{
			model->saveData();
			resultNotification->setText(Wt::WString::tr("Lms.Admin.InitWizard.done"));
			resultNotification->setHidden(false);
			saveButton->setEnabled(false);
		}

		updateView(model.get());
	});

	updateView(model.get());
}

} // namespace UserInterface

