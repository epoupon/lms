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

#include "services/auth/IPasswordService.hpp"
#include "services/database/User.hpp"
#include "services/database/Session.hpp"
#include "utils/IConfig.hpp"
#include "utils/Exception.hpp"
#include "utils/Logger.hpp"
#include "utils/Service.hpp"
#include "utils/String.hpp"

#include "common/LoginNameValidator.hpp"
#include "common/PasswordValidator.hpp"
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

		UserModel(std::optional<UserId> userId, ::Auth::IPasswordService* authPasswordService)
		: _userId {userId}
		, _authPasswordService {authPasswordService}
		{
			if (!_userId)
			{
				addField(LoginField);
				setValidator(LoginField, createLoginNameValidator());
			}

			if (authPasswordService)
			{
				addField(PasswordField);
				setValidator(PasswordField, createPasswordStrengthValidator([this] { return ::Auth::PasswordValidationContext {getLoginName(), getUserType()}; }));
				if (!userId)
					validator(PasswordField)->setMandatory(true);
			}
			addField(DemoField);

			loadData();
		}

		void saveData()
		{
			auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

			if (_userId)
			{
				// Update user
				User::pointer user {User::find(LmsApp->getDbSession(), *_userId)};
				if (!user)
					throw UserNotFoundException {};

				if (_authPasswordService && !valueText(PasswordField).empty())
					_authPasswordService->setPassword(user->getId(), valueText(PasswordField).toUTF8());
			}
			else
			{
				// Check races with other endpoints (subsonic API...)
				User::pointer user {User::find(LmsApp->getDbSession(), valueText(LoginField).toUTF8())};
				if (user)
					throw UserNotAllowedException {};

				// Create user
				user = LmsApp->getDbSession().create<User>(valueText(LoginField).toUTF8());

				if (Wt::asNumber(value(DemoField)))
					user.modify()->setType(UserType::DEMO);

				if (_authPasswordService)
					_authPasswordService->setPassword(user->getId(), valueText(PasswordField).toUTF8());
			}
		}

	private:
		void loadData()
		{
			if (!_userId)
				return;

			auto transaction {LmsApp->getDbSession().createSharedTransaction()};

			const User::pointer user {User::find(LmsApp->getDbSession(), *_userId)};
			if (!user)
				throw UserNotFoundException {};
			else if (user == LmsApp->getUser())
				throw UserNotAllowedException {};
		}

		UserType getUserType() const
		{
			if (_userId)
			{
				auto transaction {LmsApp->getDbSession().createSharedTransaction()};

				const User::pointer user {User::find(LmsApp->getDbSession(), *_userId)};
				return user->getType();
			}

			return Wt::asNumber(value(DemoField)) ? UserType::DEMO : UserType::REGULAR;
		}

		std::string getLoginName() const
		{
			if (_userId)
			{
				auto transaction {LmsApp->getDbSession().createSharedTransaction()};

				const User::pointer user {User::find(LmsApp->getDbSession(), *_userId)};
				return user->getLoginName();
			}

			return valueText(LoginField).toUTF8();
		}

		bool validateField(Field field)
		{
			Wt::WString error;

			if (field == LoginField)
			{
				auto transaction {LmsApp->getDbSession().createSharedTransaction()};

				const User::pointer user {User::find(LmsApp->getDbSession(), valueText(LoginField).toUTF8())};
				if (user)
					error = Wt::WString::tr("Lms.Admin.User.user-already-exists");
			}
			else if (field == DemoField)
			{
				auto transaction {LmsApp->getDbSession().createSharedTransaction()};

				if (Wt::asNumber(value(DemoField)) && User::findDemoUser(LmsApp->getDbSession()))
					error = Wt::WString::tr("Lms.Admin.User.demo-account-already-exists");
			}

			if (error.empty())
				return Wt::WFormModel::validateField(field);

			setValidation(field, Wt::WValidator::Result {Wt::ValidationState::Invalid, error});

			return false;
		}

		std::optional<UserId> _userId;
		::Auth::IPasswordService* _authPasswordService {};
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

	const std::optional<UserId> userId {StringUtils::readAs<UserId::ValueType>(wApp->internalPathNextPart("/admin/user/"))};

	clear();

	Wt::WTemplateFormView* t {addNew<Wt::WTemplateFormView>(Wt::WString::tr("Lms.Admin.User.template"))};

	auto* authPasswordService {Service<::Auth::IPasswordService>::get()};
	if (authPasswordService && !authPasswordService->canSetPasswords())
		authPasswordService = nullptr;

	auto model {std::make_shared<UserModel>(userId, authPasswordService)};

	if (userId)
	{
		auto transaction {LmsApp->getDbSession().createSharedTransaction()};

		const User::pointer user {User::find(LmsApp->getDbSession(), *userId)};
		if (!user)
			throw UserNotFoundException {};

		const Wt::WString title {Wt::WString::tr("Lms.Admin.User.user-edit").arg(user->getLoginName())};
		LmsApp->setTitle(title);

		t->bindString("title", title, Wt::TextFormat::Plain);
		t->setCondition("if-has-last-login", true);
		t->bindString("last-login", user->getLastLogin().toString(), Wt::TextFormat::Plain);
	}
	else
	{
		const Wt::WString title {Wt::WString::tr("Lms.Admin.User.user-create")};
		LmsApp->setTitle(title);

		// Login
		t->setCondition("if-has-login", true);
		t->setFormWidget(UserModel::LoginField, std::make_unique<Wt::WLineEdit>());
		t->bindString("title", title);
	}

	if (authPasswordService)
	{
		t->setCondition("if-has-password", true);

		// Password
		auto passwordEdit = std::make_unique<Wt::WLineEdit>();
		passwordEdit->setEchoMode(Wt::EchoMode::Password);
		passwordEdit->setAttributeValue("autocomplete", "off");
		t->setFormWidget(UserModel::PasswordField, std::move(passwordEdit));
	}

	// Demo account
	t->setFormWidget(UserModel::DemoField, std::make_unique<Wt::WCheckBox>());
	if (!userId && Service<IConfig>::get()->getBool("demo", false))
		t->setCondition("if-demo", true);

	Wt::WPushButton* saveBtn {t->bindNew<Wt::WPushButton>("save-btn", Wt::WString::tr(userId ? "Lms.save" : "Lms.create"))};
	saveBtn->clicked().connect([=]()
	{
		t->updateModel(model.get());

		if (model->validate())
		{
			model->saveData();
			LmsApp->notifyMsg(Notification::Type::Info,
					Wt::WString::tr("Lms.Admin.Users.users"),
					Wt::WString::tr(userId ? "Lms.Admin.User.user-updated" : "Lms.Admin.User.user-created"));
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


