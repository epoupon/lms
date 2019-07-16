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

#include <Wt/WApplication.h>
#include <Wt/WCheckBox.h>
#include <Wt/WComboBox.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WTemplateFormView.h>

#include <Wt/WFormModel.h>

#include "database/User.hpp"
#include "utils/Config.hpp"
#include "utils/Exception.hpp"
#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

#include "common/Validators.hpp"
#include "common/ValueStringModel.hpp"
#include "LmsApplication.hpp"

namespace UserInterface {

using namespace Database;

class UserModel : public Wt::WFormModel
{

	public:
		static const Field LoginField;
		static const Field PasswordField;
		static const Field AudioTranscodeBitrateLimitField;
		static const Field DemoField;

		UserModel(boost::optional<Database::IdType> userId)
		: Wt::WFormModel(),
		_userId(userId)
		{
			if (!_userId)
			{
				addField(LoginField);
				setValidator(LoginField, createNameValidator());
			}

			addField(PasswordField);
			addField(AudioTranscodeBitrateLimitField);
			addField(DemoField);

			if (!_userId)
				setValidator(PasswordField, createMandatoryValidator());

			initializeModels();

			// populate the model with initial data
			loadData();
		}

		std::shared_ptr<Wt::WAbstractItemModel> bitrateModel() { return _bitrateModel; }

		void saveData()
		{
			auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

			if (_userId)
			{
				// Update user
				Database::User::pointer user {Database::User::getById(LmsApp->getDbSession(), *_userId)};

				// Account
				if (!valueText(PasswordField).empty())
					LmsApp->getDbSession().updateUserPassword(user, valueText(PasswordField).toUTF8());

				auto transcodeBitrateLimitRow {_bitrateModel->getRowFromString(valueText(AudioTranscodeBitrateLimitField))};
				if (transcodeBitrateLimitRow)
					user.modify()->setMaxAudioTranscodeBitrate(_bitrateModel->getValue(*transcodeBitrateLimitRow));
			}
			else
			{
				// Create user
				Database::User::pointer user = LmsApp->getDbSession().createUser(valueText(LoginField).toUTF8(), valueText(PasswordField).toUTF8());

				auto transcodeBitrateLimitRow {_bitrateModel->getRowFromString(valueText(AudioTranscodeBitrateLimitField))};
				if (transcodeBitrateLimitRow )
					user.modify()->setMaxAudioTranscodeBitrate(_bitrateModel->getValue(*transcodeBitrateLimitRow));

				if (Wt::asNumber(value(DemoField)))
					user.modify()->setType(Database::User::Type::DEMO);
			}
		}

	private:

		void loadData()
		{
			if (!_userId)
				return;

			auto transaction {LmsApp->getDbSession().createSharedTransaction()};

			const Database::User::pointer user {Database::User::getById(LmsApp->getDbSession(), *_userId)};
			if (user == LmsApp->getUser())
				throw LmsException("Cannot edit ourselves");

			auto transcodeBitrateLimitRow {_bitrateModel->getRowFromValue(user->getMaxAudioTranscodeBitrate())};
			if (transcodeBitrateLimitRow)
				setValue(AudioTranscodeBitrateLimitField, _bitrateModel->getString(*transcodeBitrateLimitRow));
		}

		Wt::WString getLoginName() const
		{
			if (_userId)
			{
				auto transaction {LmsApp->getDbSession().createSharedTransaction()};

				const Database::User::pointer user {Database::User::getById(LmsApp->getDbSession(), *_userId)};
				return LmsApp->getDbSession().getUserLoginName(user);
			}
			else
				return valueText(LoginField);
		}

		bool validateField(Field field)
		{
			Wt::WString error;

			if (field == LoginField)
			{
				const Database::User::pointer user {LmsApp->getDbSession().getUser(valueText(LoginField).toUTF8())};
				if (user)
					error = Wt::WString::tr("Lms.Admin.User.user-already-exists");
			}
			else if (field == PasswordField)
			{
				if (!valueText(PasswordField).empty())
				{
					if (Wt::asNumber(value(DemoField)))
					{
						//Demo account: password must be the same as the login name
						if (valueText(PasswordField) != getLoginName())
							error = Wt::WString::tr("Lms.Admin.User.demo-password-invalid");
					}
					else
					{
						// Evaluate the strength of the password for non demo accounts
						auto res = Database::Session::getPasswordService().strengthValidator()->evaluateStrength(valueText(PasswordField), getLoginName(), "");

						if (!res.isValid())
							error = res.message();
					}
				}
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


		void initializeModels()
		{
			_bitrateModel = std::make_shared<ValueStringModel<Bitrate>>();

			for (auto bitrate : Database::User::audioTranscodeAllowedBitrates)
				_bitrateModel->add( Wt::WString::fromUTF8(std::to_string(bitrate / 1000)), bitrate );
		}

		std::shared_ptr<ValueStringModel<Bitrate>>	_bitrateModel;
		boost::optional<Database::IdType> _userId;
};

const Wt::WFormModel::Field UserModel::LoginField = "login";
const Wt::WFormModel::Field UserModel::PasswordField = "password";
const Wt::WFormModel::Field UserModel::AudioTranscodeBitrateLimitField = "audio-transcode-bitrate-limit";
const Wt::WFormModel::Field UserModel::DemoField = "demo";

UserView::UserView()
{
	wApp->internalPathChanged().connect(std::bind([=]
	{
		refreshView();
	}));

	refreshView();
}

void
UserView::refreshView()
{
	if (!wApp->internalPathMatches("/admin/user"))
		return;

	auto userId = readAs<Database::IdType>(wApp->internalPathNextPart("/admin/user/"));

	clear();

	Wt::WTemplateFormView* t {addNew<Wt::WTemplateFormView>(Wt::WString::tr("Lms.Admin.User.template"))};

	auto model {std::make_shared<UserModel>(userId)};

	if (userId)
	{
		auto transaction {LmsApp->getDbSession().createSharedTransaction()};

		const Database::User::pointer user {Database::User::getById(LmsApp->getDbSession(), *userId)};
		const std::string loginName {LmsApp->getDbSession().getUserLoginName(user)};
		t->bindString("title", Wt::WString::tr("Lms.Admin.User.user-edit").arg(loginName), Wt::TextFormat::Plain);
		t->setCondition("if-has-last-login-attempt", true);

		Wt::WLineEdit *lastLoginAttempt {t->bindNew<Wt::WLineEdit>("last-login-attempt")};
		lastLoginAttempt->setText(LmsApp->getDbSession().getUserLastLoginAttempt(user).toString());
		lastLoginAttempt->setEnabled(false);
	}
	else
	{
		// Login
		t->setCondition("if-has-login", true);
		t->setFormWidget(UserModel::LoginField, std::make_unique<Wt::WLineEdit>());
		t->bindString("title", Wt::WString::tr("Lms.Admin.User.user-create"));
	}

	// Password
	auto passwordEdit = std::make_unique<Wt::WLineEdit>();
	passwordEdit->setEchoMode(Wt::EchoMode::Password);
	t->setFormWidget(UserModel::PasswordField, std::move(passwordEdit));

	// Transcode bitrate limit
	auto bitrate = std::make_unique<Wt::WComboBox>();
	bitrate->setModel(model->bitrateModel());
	t->setFormWidget(UserModel::AudioTranscodeBitrateLimitField, std::move(bitrate));

	// Demo account
	t->setFormWidget(UserModel::DemoField, std::make_unique<Wt::WCheckBox>());
	if (!userId && Config::instance().getBool("demo", false))
		t->setCondition("if-demo", true);

	Wt::WPushButton* saveBtn = t->bindNew<Wt::WPushButton>("save-btn", Wt::WString::tr(userId ? "Lms.save" : "Lms.create"));
	saveBtn->clicked().connect(std::bind([=]
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
	}));

	t->updateView(model.get());
}

} // namespace UserInterface


