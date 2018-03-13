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
#include <Wt/WPushButton>
#include <Wt/Auth/Identity>

#include "utils/Logger.hpp"

#include "common/Validators.hpp"
#include "LmsApplication.hpp"

#include "FirstConnectionView.hpp"

namespace UserInterface {
namespace Settings {

class FirstConnectionModel : public Wt::WFormModel
{
	public:

		// Associate each field with a unique string literal.
		static const Field AdminLoginField;
		static const Field PasswordField;
		static const Field PasswordConfirmField;

		FirstConnectionModel(Wt::WObject *parent = 0)
			: Wt::WFormModel(parent)
		{
			addField(AdminLoginField);
			addField(PasswordField);
			addField(PasswordConfirmField);

			setValidator(AdminLoginField, createNameValidator());
			setValidator(PasswordField, createMandatoryValidator());
			setValidator(PasswordConfirmField, createMandatoryValidator());
		}

		void saveData()
		{
			Wt::Dbo::Transaction transaction(DboSession());

			// Check if a user already exist
			// If it's the case, just do nothing
			if (!Database::User::getAll(DboSession()).empty())
				throw std::runtime_error("Admin user already created");

			// Create user
			Wt::Auth::User authUser = DbHandler().getUserDatabase().registerNew();
			Database::User::pointer user = DbHandler().getUser(authUser);

			// Account
			authUser.setIdentity(Wt::Auth::Identity::LoginName, valueText(AdminLoginField));
			Database::Handler::getPasswordService().updatePassword(authUser, valueText(PasswordField));

			user.modify()->setAdmin( true );
		}

		bool validateField(Field field)
		{
			Wt::WString error;

			if (field == PasswordField)
			{
				// Password is mandatory if we create the user
				if (!valueText(PasswordField).empty())
				{
					// Evaluate the strength of the password
					Wt::Auth::AbstractPasswordService::StrengthValidatorResult res
						= Database::Handler::getPasswordService().strengthValidator()->evaluateStrength(valueText(PasswordField),
								valueText(AdminLoginField), "");

					if (!res.isValid())
						error = res.message();
				}
				else
					return Wt::WFormModel::validateField(field);
			}
			else if (field == PasswordConfirmField)
			{
				if (validation(PasswordField).state() == Wt::WValidator::Valid)
				{
					if (valueText(PasswordField) != valueText(PasswordConfirmField))
						error = Wt::WString::tr("Wt.Auth.passwords-dont-match");
				}
			}
			else
			{
				return Wt::WFormModel::validateField(field);
			}

			setValidation(field, Wt::WValidator::Result( error.empty() ? Wt::WValidator::Valid : Wt::WValidator::Invalid, error));

			return (validation(field).state() == Wt::WValidator::Valid);
		}

};

const Wt::WFormModel::Field FirstConnectionModel::AdminLoginField = "admin-login";
const Wt::WFormModel::Field FirstConnectionModel::PasswordField = "password";
const Wt::WFormModel::Field FirstConnectionModel::PasswordConfirmField = "password-confirm";

FirstConnectionView::FirstConnectionView(Wt::WContainerWidget *parent)
: Wt::WTemplateFormView(parent)
{
	auto model = new FirstConnectionModel(this);

	setTemplateText(Wt::WString::tr("template-settings-first-connection"));
	addFunction("tr", &WTemplate::Functions::tr);
	addFunction("id", &WTemplate::Functions::id);

	// AdminLogin
	Wt::WLineEdit* accountEdit = new Wt::WLineEdit();
	setFormWidget(FirstConnectionModel::AdminLoginField, accountEdit);

	// Password
	Wt::WLineEdit* passwordEdit = new Wt::WLineEdit();
	setFormWidget(FirstConnectionModel::PasswordField, passwordEdit );
	passwordEdit->setEchoMode(Wt::WLineEdit::Password);

	// Password confirmation
	Wt::WLineEdit* passwordConfirmEdit = new Wt::WLineEdit();
	setFormWidget(FirstConnectionModel::PasswordConfirmField, passwordConfirmEdit);
	passwordConfirmEdit->setEchoMode(Wt::WLineEdit::Password);

	auto saveButton = new Wt::WPushButton(Wt::WString::tr("msg-create"));
	bindWidget("create-btn", saveButton);
	saveButton->clicked().connect(std::bind([=]
	{
		updateModel(model);

		if (model->validate())
		{
			model->saveData();
			LmsApp->notify(Wt::WString::tr("msg-settings-first-connection-done"));
			saveButton->setEnabled(false);
		}

		updateView(model);
	}));

	updateView(model);
}

} // namespace Settings
} // namespace UserInterface

