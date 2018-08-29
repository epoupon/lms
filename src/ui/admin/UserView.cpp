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
#include <Wt/WStringListModel.h>

#include "common/Validators.hpp"
#include "utils/Config.hpp"
#include "utils/Exception.hpp"
#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

#include "LmsApplication.hpp"

namespace UserInterface {


class UserModel : public Wt::WFormModel
{
	public:
		static const Field LoginField;
		static const Field PasswordField;
		static const Field BitrateLimitField;
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
			addField(BitrateLimitField);
			addField(DemoField);

			if (!_userId)
				setValidator(PasswordField, createMandatoryValidator());

			initializeModels();

			// populate the model with initial data
			loadData();
		}

		void loadData()
		{
			if (!_userId)
				return;

			Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

			auto authUser = LmsApp->getDb().getUserDatabase().findWithId( std::to_string(*_userId) );
			auto user = LmsApp->getDb().getUser(authUser);

			if (user == LmsApp->getUser())
				throw LmsException("Cannot edit ourselves");

			auto bitrate = getBitrateLimitRow(user->getMaxAudioBitrate());
			if (bitrate)
				setValue(BitrateLimitField, bitrateLimitString(*bitrate));
		}

		void saveData()
		{
			Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

			if (_userId)
			{
				// Update user
				Wt::Auth::User authUser = LmsApp->getDb().getUserDatabase().findWithId( std::to_string(*_userId) );
				Database::User::pointer user = LmsApp->getDb().getUser( authUser );

				// Account
				if (!valueText(PasswordField).empty())
					Database::Handler::getPasswordService().updatePassword(authUser, valueText(PasswordField));

				auto bitrateLimitRow = getBitrateLimitRow(Wt::asString(value(BitrateLimitField)));
				user.modify()->setMaxAudioBitrate(bitrateLimit(*bitrateLimitRow));
				LMS_LOG(UI, DEBUG) << "Max audio bitrate set to " << bitrateLimit(*bitrateLimitRow);
			}
			else
			{
				// Create user
				Wt::Auth::User authUser = LmsApp->getDb().getUserDatabase().registerNew();
				Database::User::pointer user = LmsApp->getDb().createUser(authUser);

				// Account
				authUser.setIdentity(Wt::Auth::Identity::LoginName, valueText(LoginField));
				Database::Handler::getPasswordService().updatePassword(authUser, valueText(PasswordField));

				auto bitrateLimitRow = getBitrateLimitRow(Wt::asString(value(BitrateLimitField)));
				user.modify()->setMaxAudioBitrate(bitrateLimit(*bitrateLimitRow));

				if (Wt::asNumber(value(DemoField)))
					user.modify()->setType(Database::User::Type::DEMO);
			}
		}

		Wt::WString getLogin() const
		{
			if (_userId)
			{
				auto authUser = LmsApp->getDb().getUserDatabase().findWithId( std::to_string(*_userId) );
				return authUser.identity(Wt::Auth::Identity::LoginName);
			}
			else
				return valueText(LoginField);
		}

		bool validateField(Field field)
		{
			Wt::WString error;

			if (field == LoginField)
			{
				auto user = LmsApp->getDb().getUserDatabase().findWithIdentity(Wt::Auth::Identity::LoginName, valueText(LoginField));
				if (user.isValid())
					error = Wt::WString::tr("Lms.Admin.User.user-already-exists");
			}
			else if (field == PasswordField)
			{
				if (!valueText(PasswordField).empty())
				{
					if (Wt::asNumber(value(DemoField)))
					{
						//Demo account: password must be the same as the login name
						if (valueText(PasswordField) != getLogin())
							error = Wt::WString::tr("Lms.Admin.User.demo-password-invalid");
					}
					else
					{
						// Evaluate the strength of the password for non demo accounts
						auto res = Database::Handler::getPasswordService().strengthValidator()->evaluateStrength(valueText(PasswordField), getLogin(), "");

						if (!res.isValid())
							error = res.message();
					}
				}
			}
			else if (field == DemoField)
			{
				Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

				if (Wt::asNumber(value(DemoField)) && Database::User::getDemo(LmsApp->getDboSession()))
					error = Wt::WString::tr("Lms.Admin.User.demo-account-already-exists");
			}

			if (error.empty())
				return Wt::WFormModel::validateField(field);

			setValidation(field, Wt::WValidator::Result( Wt::ValidationState::Invalid, error));

			return false;
		}

		boost::optional<int> getBitrateLimitRow(Wt::WString value)
		{
			for (int i = 0; i < _bitrateModel->rowCount(); ++i)
			{
				if (bitrateLimitString(i) == value)
					return i;
			}

			return boost::none;
		}

		boost::optional<int> getBitrateLimitRow(std::size_t value)
		{
			for (int i = 0; i < _bitrateModel->rowCount(); ++i)
			{
				if (bitrateLimit(i) == value)
					return i;
			}

			return boost::none;
		}

		std::size_t bitrateLimit(int row)
		{
			return Wt::cpp17::any_cast<std::size_t>
				(_bitrateModel->data(_bitrateModel->index(row, 0), Wt::ItemDataRole::User));
		}

		Wt::WString bitrateLimitString(int row)
		{
			return Wt::cpp17::any_cast<Wt::WString>
				(_bitrateModel->data(_bitrateModel->index(row, 0), Wt::ItemDataRole::Display));
		}

		std::shared_ptr<Wt::WAbstractItemModel> bitrateModel() { return _bitrateModel; }

	private:

		void initializeModels()
		{
			_bitrateModel = std::make_shared<Wt::WStringListModel>();

			std::size_t id = 0;
			for (auto bitrate : Database::User::audioBitrates)
			{
				_bitrateModel->addString( Wt::WString::fromUTF8(std::to_string(bitrate / 1000)) );
				_bitrateModel->setData( id++, 0, bitrate, Wt::ItemDataRole::User);
			}


		}

		std::shared_ptr<Wt::WStringListModel>	_bitrateModel;
		boost::optional<Database::IdType> _userId;
};

const Wt::WFormModel::Field UserModel::LoginField = "login";
const Wt::WFormModel::Field UserModel::PasswordField = "password";
const Wt::WFormModel::Field UserModel::BitrateLimitField = "audio-bitrate-limit";
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

	LMS_LOG(UI, DEBUG) << "userId = " << (userId ? std::to_string(*userId) : "none");

	clear();

	Wt::WTemplateFormView* t = addNew<Wt::WTemplateFormView>(Wt::WString::tr("Lms.Admin.User.template"));

	auto model = std::make_shared<UserModel>(userId);

	if (userId)
	{
		auto authUser = LmsApp->getDb().getUserDatabase().findWithId( std::to_string(*userId) );
		auto name = authUser.identity(Wt::Auth::Identity::LoginName);
		t->bindString("title", Wt::WString::tr("Lms.Admin.User.user-edit").arg(name), Wt::TextFormat::Plain);
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

	// Bitrate
	auto bitrate = std::make_unique<Wt::WComboBox>();
	bitrate->setModel(model->bitrateModel());
	t->setFormWidget(UserModel::BitrateLimitField, std::move(bitrate));

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


