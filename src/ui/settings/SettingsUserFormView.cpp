/*
 * Copyright (C) 2013 Emeric Poupon
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

#include <vector>

#include <Wt/WStringListModel>
#include <Wt/WFormModel>
#include <Wt/WLineEdit>
#include <Wt/WCheckBox>
#include <Wt/WComboBox>
#include <Wt/WPushButton>
#include <Wt/Auth/Identity>

#include "logger/Logger.hpp"

#include "common/Validators.hpp"

#include "SettingsUserFormView.hpp"


namespace UserInterface {
namespace Settings {

class UserFormModel : public Wt::WFormModel
{
	public:

		// Associate each field with a unique string literal.
		static const Field NameField;
		static const Field EmailField;
		static const Field PasswordField;
		static const Field PasswordConfirmField;
		static const Field AdminField;
		static const Field AudioBitrateLimitField;
		static const Field VideoBitrateLimitField;

		UserFormModel(SessionData& sessionData, std::string userId, Wt::WObject *parent = 0)
			: Wt::WFormModel(parent),
			_db(sessionData.getDatabaseHandler()),
			_userId(userId)
		{
			initializeModels();

			addField(NameField);
			addField(EmailField);
			addField(PasswordField);
			addField(PasswordConfirmField);
			addField(AdminField);
			addField(AudioBitrateLimitField);
			addField(VideoBitrateLimitField);

			setValidator(NameField, createNameValidator());
			setValidator(EmailField, createEmailValidator());
			// If creating a user, passwords are mandatory
			if (_userId.empty())
			{
				setValidator(PasswordField, new Wt::WValidator(true)); // mandatory
				setValidator(PasswordConfirmField, new Wt::WValidator(true)); // mandatory
			}
			setValidator(AudioBitrateLimitField, new Wt::WValidator(true)); // mandatory
			setValidator(VideoBitrateLimitField, new Wt::WValidator(true)); // mandatory

			// populate the model with initial data
			loadData(userId);
		}

		Wt::WAbstractItemModel *audioBitrateModel() { return _audioBitrateModel; }
		Wt::WAbstractItemModel *videoBitrateModel() { return _videoBitrateModel; }

		void loadData(std::string userId)
		{

			if (!userId.empty())
			{
				Wt::Dbo::Transaction transaction(_db.getSession());

				Wt::Auth::User authUser = _db.getUserDatabase().findWithId( userId );
				Database::User::pointer user = _db.getUser(authUser);

				Wt::Auth::User currentUser = _db.getLogin().user();

				if (user && authUser.isValid())
				{
					if (user->isAdmin())
					{
						setValue(AdminField, true);

						// We can cannot remove admin rights to ourselves
						if (currentUser == authUser)
							setReadOnly(AdminField, true);

						// if the user is admin, no need to limit it
						setReadOnly(AudioBitrateLimitField, true);
						setValidator(AudioBitrateLimitField, nullptr);

						setReadOnly(VideoBitrateLimitField, true);
						setValidator(VideoBitrateLimitField, nullptr);
					}
					else
					{
						setValue(AudioBitrateLimitField, user->getMaxAudioBitrate() / 1000); // in kbps
						setValue(VideoBitrateLimitField, user->getMaxVideoBitrate() / 1000); // in kbps
					}

					setValue(NameField, authUser.identity(Wt::Auth::Identity::LoginName));
					if (!authUser.email().empty())
						setValue(EmailField, authUser.email());
					else
						setValue(EmailField, authUser.unverifiedEmail());
				}
			}
		}

		bool saveData()
		{
			try {
				Wt::Dbo::Transaction transaction(_db.getSession());

				if (_userId.empty())
				{
					// Create user
					Wt::Auth::User authUser = _db.getUserDatabase().registerNew();
					Database::User::pointer user = _db.getUser(authUser);

					// Account
					authUser.setIdentity(Wt::Auth::Identity::LoginName, valueText(NameField));
					authUser.setEmail(valueText(EmailField).toUTF8());
					Database::Handler::getPasswordService().updatePassword(authUser, valueText(PasswordField));

					// Access
					{
						boost::any v = value(AdminField);
						if (!v.empty() && boost::any_cast<bool>(v) == true)
							user.modify()->setAdmin( true );
						else
							user.modify()->setAdmin( false );
					}

					if (!isReadOnly(AudioBitrateLimitField))
						user.modify()->setMaxAudioBitrate( Wt::asNumber(value(AudioBitrateLimitField)) * 1000); // in bps

					if (!isReadOnly(VideoBitrateLimitField))
						user.modify()->setMaxVideoBitrate( Wt::asNumber(value(VideoBitrateLimitField)) * 1000); // in bps

					transaction.commit();
				}
				else
				{
					// Update user
					Wt::Auth::User authUser = _db.getUserDatabase().findWithId(_userId);
					Database::User::pointer user = _db.getUser( authUser );

					// user may have been deleted by someone else
					if (!authUser.isValid()) {
						LMS_LOG(MOD_UI, SEV_ERROR) << "user identity does not exist!";
						return false;
					}
					else if(!user)
					{
						LMS_LOG(MOD_UI, SEV_ERROR) << "User not found!";
						return false;
					}

					// Account
					authUser.setIdentity(Wt::Auth::Identity::LoginName, valueText(NameField));
					authUser.setEmail(valueText(EmailField).toUTF8());

					// Password
					if (!valueText(PasswordField).empty())
						Database::Handler::getPasswordService().updatePassword(authUser, valueText(PasswordField));

					// Access
					if (!isReadOnly(AdminField))
					{
						boost::any v = value(AdminField);
						if (!v.empty() && boost::any_cast<bool>(v) == true)
							user.modify()->setAdmin( true );
						else
							user.modify()->setAdmin( false );
					}

					if (!isReadOnly(AudioBitrateLimitField))
						user.modify()->setMaxAudioBitrate( Wt::asNumber(value(AudioBitrateLimitField)) * 1000); // in bps

					if (!isReadOnly(VideoBitrateLimitField))
						user.modify()->setMaxVideoBitrate( Wt::asNumber(value(VideoBitrateLimitField)) * 1000); // in bps

					transaction.commit();
				}
			}
			catch(Wt::Dbo::Exception& exception)
			{
				LMS_LOG(MOD_UI, SEV_ERROR) << "Dbo exception: " << exception.what();
				return false;
			}

			return true;
		}

		bool validateField(Field field)
		{
			Wt::WString error;

			if (field == NameField)
			{
				Wt::Dbo::Transaction transaction(_db.getSession());
				// Must be unique since used as LoginIdentity
				Wt::Auth::User user = _db.getUserDatabase().findWithIdentity(Wt::Auth::Identity::LoginName, valueText(field));
				if (user.isValid() && user.id() != _userId)
					error = "Already exists";
				else
					return Wt::WFormModel::validateField(field);
			}
			else if (field == PasswordField)
			{
				// Password is mandatory if we create the user
				if (!valueText(PasswordField).empty())
				{
					// Evaluate the strength of the password
					Wt::Auth::AbstractPasswordService::StrengthValidatorResult res
						= Database::Handler::getPasswordService().strengthValidator()->evaluateStrength(valueText(PasswordField),
								valueText(NameField),
								valueText(EmailField).toUTF8());

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
				// Apply validators
				return Wt::WFormModel::validateField(field);
			}

			setValidation(field, Wt::WValidator::Result( error.empty() ? Wt::WValidator::Valid : Wt::WValidator::Invalid, error));

			return validation(field).state() == Wt::WValidator::Valid;
		}


	private:

		void initializeModels()
		{
			// AUDIO
			_audioBitrateModel = new Wt::WStringListModel();
			BOOST_FOREACH(std::size_t bitrate, Database::User::audioBitrates)
				_audioBitrateModel->addString( Wt::WString("{1}").arg( bitrate / 1000 ) ); // in kbps

			// VIDEO
			_videoBitrateModel = new Wt::WStringListModel();
			BOOST_FOREACH(std::size_t bitrate, Database::User::videoBitrates)
				_videoBitrateModel->addString( Wt::WString("{1}").arg( bitrate / 1000 ) ); // in kbps

		}

		Database::Handler&	_db;
		std::string		_userId;
		Wt::WStringListModel*	_audioBitrateModel;
		Wt::WStringListModel*	_videoBitrateModel;
};

const Wt::WFormModel::Field UserFormModel::NameField = "name";
const Wt::WFormModel::Field UserFormModel::EmailField = "email";
const Wt::WFormModel::Field UserFormModel::PasswordField = "password";
const Wt::WFormModel::Field UserFormModel::PasswordConfirmField = "password-confirm";
const Wt::WFormModel::Field UserFormModel::AdminField = "admin";
const Wt::WFormModel::Field UserFormModel::AudioBitrateLimitField = "audio-bitrate-limit";
const Wt::WFormModel::Field UserFormModel::VideoBitrateLimitField = "video-bitrate-limit";


UserFormView::UserFormView(SessionData& sessionData, std::string userId, Wt::WContainerWidget *parent)
: Wt::WTemplateFormView(parent)
{

	_model = new UserFormModel(sessionData, userId, this);

	setTemplateText(tr("userForm-template"));
	addFunction("id", &WTemplate::Functions::id);
	addFunction("block", &WTemplate::Functions::id);

	// Name
	setFormWidget(UserFormModel::NameField, new Wt::WLineEdit());

	// Email
	setFormWidget(UserFormModel::EmailField, new Wt::WLineEdit());

	// Password
	Wt::WLineEdit* passwordEdit = new Wt::WLineEdit();
	setFormWidget(UserFormModel::PasswordField, passwordEdit );
	passwordEdit->setEchoMode(Wt::WLineEdit::Password);

	// Password confirmation
	Wt::WLineEdit* passwordConfirmEdit = new Wt::WLineEdit();
	setFormWidget(UserFormModel::PasswordConfirmField, passwordConfirmEdit);
	passwordConfirmEdit->setEchoMode(Wt::WLineEdit::Password);

	bindString("access", "Access");

	// Admin Field
	setFormWidget(UserFormModel::AdminField, new Wt::WCheckBox());

	// AudioBitrate
	Wt::WComboBox *audioBitrateCB = new Wt::WComboBox();
	setFormWidget(UserFormModel::AudioBitrateLimitField, audioBitrateCB);
	audioBitrateCB->setStyleClass("span2");
	audioBitrateCB->setModel(_model->audioBitrateModel());

	// VideoBitrate
	Wt::WComboBox *videoBitrateCB = new Wt::WComboBox();
	setFormWidget(UserFormModel::VideoBitrateLimitField, videoBitrateCB);
	videoBitrateCB->setStyleClass("span2");
	videoBitrateCB->setModel(_model->videoBitrateModel());

	// Title & Buttons
	Wt::WString title;
	if (userId.empty()) {
		title = Wt::WString("Create user");
	}
	else {
		Database::Handler &db = sessionData.getDatabaseHandler();
		Wt::Dbo::Transaction transaction (db.getSession());
		Wt::Auth::User authUser = db.getUserDatabase().findWithId( userId );

		Wt::WString userName;
		if (authUser.isValid())
			userName = authUser.identity(Wt::Auth::Identity::LoginName);
		else
			; // TODO display user deleted and close the widget

		title = Wt::WString("Edit user {1}").arg(userName);
	}

	bindString("title", title);

	Wt::WPushButton *saveButton = new Wt::WPushButton();
	if (userId.empty()) {
		saveButton->setText("Create user");
		saveButton->setStyleClass("btn-success");
	}
	else
	{
		saveButton->setText("Save");
		saveButton->setStyleClass("btn-primary");
	}
	bindWidget("save-button", saveButton);
	saveButton->clicked().connect(this, &UserFormView::processSave);

	Wt::WPushButton *cancelButton = new Wt::WPushButton("Cancel");
	bindWidget("cancel-button", cancelButton);
	cancelButton->clicked().connect(this, &UserFormView::processCancel);

	updateView(_model);

}

	void
UserFormView::processCancel()
{
	// parent widget will delete this widget
	completed().emit(false);
}

void
UserFormView::processSave()
{
	updateModel(_model);


	if (_model->validate())
	{
		// commit model into DB
		if (_model->saveData() )
		{
			// parent widget will delete this widget
			completed().emit(true);
		}
		// else TODO display a nice error message
	}
	else
	{
		updateView(_model);
	}
}

} // namespace Settings
} // namespace UserInterface
