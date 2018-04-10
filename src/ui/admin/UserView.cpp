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

#include <Wt/WApplication>
#include <Wt/WCheckBox>
#include <Wt/WComboBox>
#include <Wt/WLineEdit>
#include <Wt/WPushButton>
#include <Wt/WTemplateFormView>

#include <Wt/WFormModel>
#include <Wt/WStringListModel>

#include "common/Validators.hpp"
#include "utils/Utils.hpp"
#include "utils/Logger.hpp"

#include "LmsApplication.hpp"
#include "UserView.hpp"

namespace UserInterface {


class UserModel : public Wt::WFormModel
{
	public:
		static const Field LoginField;
		static const Field PasswordField;
		static const Field BitrateLimitField;

		UserModel(boost::optional<Database::User::id_type> userId, Wt::WObject *parent = 0)
		: Wt::WFormModel(parent),
		_userId(userId)
		{
			if (!_userId)
			{
				addField(LoginField);
				setValidator(LoginField, createNameValidator());
			}

			addField(PasswordField);
			addField(BitrateLimitField);


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

			Wt::Dbo::Transaction transaction(DboSession());

			Wt::Auth::User authUser = DbHandler().getUserDatabase().findWithId( std::to_string(*_userId) );
			Database::User::pointer user = DbHandler().getUser(authUser);

			if (user == CurrentUser())
				throw std::runtime_error("Cannot edit ourselves");

			auto bitrate = getBitrateLimitRow(user->getMaxAudioBitrate());
			if (bitrate)
				setValue(BitrateLimitField, bitrateLimitString(*bitrate));
		}

		void saveData()
		{
			Wt::Dbo::Transaction transaction(DboSession());

			if (_userId)
			{
				// Update user
				Wt::Auth::User authUser = DbHandler().getUserDatabase().findWithId( std::to_string(*_userId) );
				Database::User::pointer user = DbHandler().getUser( authUser );

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
				Wt::Auth::User authUser = DbHandler().getUserDatabase().registerNew();
				Database::User::pointer user = DbHandler().createUser(authUser);

				// Account
				authUser.setIdentity(Wt::Auth::Identity::LoginName, valueText(LoginField));
				Database::Handler::getPasswordService().updatePassword(authUser, valueText(PasswordField));

				auto bitrateLimitRow = getBitrateLimitRow(Wt::asString(value(BitrateLimitField)));
				user.modify()->setMaxAudioBitrate(bitrateLimit(*bitrateLimitRow));
			}
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
			return boost::any_cast<std::size_t>
				(_bitrateModel->data(_bitrateModel->index(row, 0), Wt::UserRole));
		}

		Wt::WString bitrateLimitString(int row)
		{
			return boost::any_cast<Wt::WString>
				(_bitrateModel->data(_bitrateModel->index(row, 0), Wt::DisplayRole));
		}

		Wt::WAbstractItemModel *bitrateModel() { return _bitrateModel; }

	private:

		void initializeModels()
		{
			_bitrateModel = new Wt::WStringListModel(this);

			std::size_t id = 0;
			for (auto bitrate : Database::User::audioBitrates)
			{
				_bitrateModel->addString( Wt::WString::fromUTF8(std::to_string(bitrate / 1000)) );
				_bitrateModel->setData( id++, 0, bitrate, Wt::UserRole);
			}


		}

		Wt::WStringListModel*	_bitrateModel;
		boost::optional<Database::User::id_type> _userId;
};

const Wt::WFormModel::Field UserModel::LoginField = "login";
const Wt::WFormModel::Field UserModel::PasswordField = "password";
const Wt::WFormModel::Field UserModel::BitrateLimitField = "audio-bitrate-limit";

UserView::UserView(Wt::WContainerWidget* parent)
: Wt::WContainerWidget(parent)
{
	wApp->internalPathChanged().connect(std::bind([=]
	{
		refresh();
	}));

	refresh();
}

void
UserView::refresh()
{
	if (!wApp->internalPathMatches("/admin/user"))
		return;

	auto userId = readLong(wApp->internalPathNextPart("/admin/user/"));

	clear();

	auto t = new Wt::WTemplateFormView(Wt::WString::tr("Lms.Admin.User.template"), this);

	t->addFunction("tr", &Wt::WTemplate::Functions::tr);
	t->addFunction("id", &Wt::WTemplate::Functions::id);

	auto model = new UserModel(userId ? boost::make_optional<Database::User::id_type>(*userId) : boost::none, this);

	if (userId)
	{
		auto authUser = DbHandler().getUserDatabase().findWithId( std::to_string(*userId) );
		auto name = authUser.identity(Wt::Auth::Identity::LoginName);
		t->bindString("title", Wt::WString::tr("Lms.Admin.User.user-edit").arg(name), Wt::PlainText);
	}
	else
	{
		// Login
		t->setCondition("if-has-login", true);
		t->setFormWidget(UserModel::LoginField, new Wt::WLineEdit());
		t->bindString("title", Wt::WString::tr("Lms.Admin.User.user-create"));
	}


	// Password
	Wt::WLineEdit* passwordEdit = new Wt::WLineEdit();
	t->setFormWidget(UserModel::PasswordField, passwordEdit );
	passwordEdit->setEchoMode(Wt::WLineEdit::Password);

	// AudioBitrate
	Wt::WComboBox *bitrateCB = new Wt::WComboBox();
	t->setFormWidget(UserModel::BitrateLimitField, bitrateCB);
	bitrateCB->setModel(model->bitrateModel());

	auto saveBtn = new Wt::WPushButton(Wt::WString::tr(userId ? "Lms.save" : "Lms.create"));
	t->bindWidget("save-btn", saveBtn);
	saveBtn->clicked().connect(std::bind([=]
	{
		t->updateModel(model);

		if (model->validate())
		{
			model->saveData();
			LmsApp->notifyMsg(Wt::WString::tr(userId ? "Lms.Admin.User.user-updated" : "Lms.Admin.User.user-created"));
			LmsApp->setInternalPath("/admin/users", true);
		}
		else
		{
			t->updateView(model);
		}
	}));

	t->updateView(model);

}

} // namespace UserInterface


