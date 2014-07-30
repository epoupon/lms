#include <vector>

#include <Wt/WStringListModel>
#include <Wt/WFormModel>
#include <Wt/WLengthValidator>
#include <Wt/WLineEdit>
#include <Wt/WCheckBox>
#include <Wt/WComboBox>
#include <Wt/WPushButton>
#include <Wt/Auth/Identity>
#include <Wt/Auth/PasswordStrengthValidator>
#include <Wt/Auth/AbstractPasswordService>
#include <Wt/Auth/PasswordVerifier>

#include "common/EmailValidator.hpp"

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
			_sessionData(sessionData),
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
			Wt::WValidator *emailValidator = createEmailValidator();
			emailValidator->setMandatory(true);
			setValidator(EmailField, emailValidator);
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
				Database::Handler& db = _sessionData.getDatabaseHandler();

				Wt::Dbo::Transaction transaction(db.getSession());

				Wt::Auth::User authUser = db.getUserDatabase().findWithId( userId );
				Database::User::pointer user = db.getUser(authUser);

				Wt::Auth::User currentUser = _sessionData.getDatabaseHandler().getLogin().user();

				if (user && user->isAdmin())
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


				if (user && authUser.isValid())
				{
					setValue(NameField, authUser.identity(Wt::Auth::Identity::LoginName));
					if (!authUser.email().empty())
						setValue(EmailField, authUser.email());
					else
						setValue(EmailField, authUser.unverifiedEmail());
				}

				if (user)
				{
					setValue(AudioBitrateLimitField, user->getMaxAudioBitrate() / 1000); // in kbps
					setValue(VideoBitrateLimitField, user->getMaxVideoBitrate() / 1000); // in kbps
				}

			}
		}

		bool saveData()
		{
			// DBO transaction active here
			try {
				Database::Handler& db = _sessionData.getDatabaseHandler();
				Wt::Dbo::Transaction transaction(db.getSession());

				if (_userId.empty())
				{
					// Create user
					Wt::Auth::User authUser = db.getUserDatabase().registerNew();
					Database::User::pointer user = db.getUser(authUser);

					// Account
					authUser.setIdentity(Wt::Auth::Identity::LoginName, valueText(NameField));
					authUser.setEmail(valueText(EmailField).toUTF8());
					db.getPasswordService().updatePassword(authUser, valueText(PasswordField));

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
					Wt::Auth::User authUser = db.getUserDatabase().findWithId(_userId);
					Database::User::pointer user = db.getUser( authUser );

					// user may have been deleted by someone else
					if (!authUser.isValid()) {
						std::cerr << "user identity does not exist!" << std::endl;
						return false;
					}
					else if(!user)
					{
						std::cerr << "User not found!" << std::endl;
						return false;
					}

					// Account
					authUser.setIdentity(Wt::Auth::Identity::LoginName, valueText(NameField));
					authUser.setEmail(valueText(EmailField).toUTF8());

					// Password
					if (!valueText(PasswordField).empty())
						db.getPasswordService().updatePassword(authUser, valueText(PasswordField));

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
				std::cerr << "Dbo exception: " << exception.what() << std::endl;
				return false;
			}

			return true;
		}

		bool validateField(Field field)
		{
			// DBO transaction active here

			Wt::WString error;

			if (field == NameField)
			{
				Wt::Dbo::Transaction transaction(_sessionData.getDatabaseHandler().getSession());
				// Must be unique since used as LoginIdentity
				Wt::Auth::User user = _sessionData.getDatabaseHandler().getUserDatabase().findWithIdentity(Wt::Auth::Identity::LoginName, valueText(field));
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
					Wt::Auth::PasswordStrengthValidator validator;

					// Reduce some constraints...
					validator.setMinimumLength( Wt::Auth::PasswordStrengthValidator::TwoCharClass, 11);
					validator.setMinimumLength( Wt::Auth::PasswordStrengthValidator::ThreeCharClass, 8 );
					validator.setMinimumLength( Wt::Auth::PasswordStrengthValidator::FourCharClass, 6  );

					Wt::Auth::AbstractPasswordService::StrengthValidatorResult res
						= validator.evaluateStrength(valueText(PasswordField),
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

		static Wt::WValidator *createNameValidator() {
			Wt::WLengthValidator *v = new Wt::WLengthValidator();
			v->setMandatory(true);
			v->setMinimumLength(3);
			v->setMaximumLength(::Database::User::MaxNameLength);
			return v;
		}

		void initializeModels()
		{

			// AUDIO
			// TODO move defaults somewhere else?
			static const std::vector<std::size_t>
				audioBitrateLimits =
				{
					64,
					96,
					128,
					160,
					192,
					224,
					256,
					320,
					512
				};

			_audioBitrateModel = new Wt::WStringListModel();
			for (std::size_t i = 0; i < audioBitrateLimits.size(); ++i)
				_audioBitrateModel->addString( Wt::WString("{1}").arg( audioBitrateLimits[i] ) );


			// VIDEO
			// TODO move defaults somewhere else?
			static const std::vector<std::size_t>
				videoBitrateLimits =
				{
					256,
					512,
					1024,
					2048,
					4096,
					8192
				};

			_videoBitrateModel = new Wt::WStringListModel();
			for (std::size_t i = 0; i < videoBitrateLimits.size(); ++i)
				_videoBitrateModel->addString( Wt::WString("{1}").arg( videoBitrateLimits[i] ) );

		}

		SessionData&		_sessionData;
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
: Wt::WTemplateFormView(parent),
 _sessionData(sessionData)
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

	// Admin Field
	Wt::WCheckBox *admin = new Wt::WCheckBox();
	setFormWidget(UserFormModel::AdminField, admin);

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
