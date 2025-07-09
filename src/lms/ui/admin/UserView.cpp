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

#include "core/IConfig.hpp"
#include "core/Service.hpp"
#include "core/String.hpp"
#include "database/Session.hpp"
#include "database/objects/User.hpp"
#include "services/auth/IAuthTokenService.hpp"
#include "services/auth/IPasswordService.hpp"

#include "LmsApplication.hpp"
#include "LmsApplicationException.hpp"
#include "common/LoginNameValidator.hpp"
#include "common/PasswordValidator.hpp"

namespace lms::ui
{
    using namespace db;

    class UserModel : public Wt::WFormModel
    {
    public:
        static inline const Field LoginField{ "login" };
        static inline const Field PasswordField{ "password" };
        static inline const Field DemoField{ "demo" };

        UserModel(std::optional<UserId> userId, auth::IPasswordService* authPasswordService, auth::IAuthTokenService& authTokenService)
            : _userId{ userId }
            , _authPasswordService{ authPasswordService }
            , _authTokenService{ authTokenService }
        {
            if (!_userId)
            {
                addField(LoginField);
                setValidator(LoginField, createLoginNameValidator());
            }

            if (authPasswordService)
            {
                addField(PasswordField);
                setValidator(PasswordField, createPasswordStrengthValidator(*authPasswordService, [this] { return auth::PasswordValidationContext{ getLoginName(), getUserType() }; }));
                if (!userId)
                    validator(PasswordField)->setMandatory(true);
            }
            addField(DemoField);

            loadData();
        }

        void saveData()
        {
            auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

            if (_userId)
            {
                // Update user
                User::pointer user{ User::find(LmsApp->getDbSession(), *_userId) };
                if (!user)
                    throw UserNotFoundException{};

                if (_authPasswordService && !valueText(PasswordField).empty())
                {
                    _authPasswordService->setPassword(user->getId(), valueText(PasswordField).toUTF8());
                    _authTokenService.clearAuthTokens("ui", user->getId());
                }
            }
            else
            {
                // Check races with other endpoints (subsonic API...)
                User::pointer user{ User::find(LmsApp->getDbSession(), valueText(LoginField).toUTF8()) };
                if (user)
                    throw UserNotAllowedException{};

                // Create user
                user = LmsApp->getDbSession().create<User>(valueText(LoginField).toUTF8());

                if (Wt::asNumber(value(DemoField)))
                {
                    user.modify()->setType(UserType::DEMO);

                    // For demo user, we create the subsonic API auth token now as we have no other mean to create it later
                    core::Service<auth::IAuthTokenService>::get()->createAuthToken("subsonic", user->getId(), core::UUID::generate().getAsString());
                }

                if (_authPasswordService)
                    _authPasswordService->setPassword(user->getId(), valueText(PasswordField).toUTF8());
            }
        }

    private:
        void loadData()
        {
            if (!_userId)
                return;

            auto transaction{ LmsApp->getDbSession().createReadTransaction() };

            const User::pointer user{ User::find(LmsApp->getDbSession(), *_userId) };
            if (!user)
                throw UserNotFoundException{};
            if (user == LmsApp->getUser())
                throw UserNotAllowedException{};
        }

        UserType getUserType() const
        {
            if (_userId)
            {
                auto transaction{ LmsApp->getDbSession().createReadTransaction() };

                const User::pointer user{ User::find(LmsApp->getDbSession(), *_userId) };
                return user->getType();
            }

            return Wt::asNumber(value(DemoField)) ? UserType::DEMO : UserType::REGULAR;
        }

        std::string getLoginName() const
        {
            if (_userId)
            {
                auto transaction{ LmsApp->getDbSession().createReadTransaction() };

                const User::pointer user{ User::find(LmsApp->getDbSession(), *_userId) };
                return user->getLoginName();
            }

            return valueText(LoginField).toUTF8();
        }

        bool validateField(Field field) override
        {
            Wt::WString error;

            if (field == LoginField)
            {
                auto transaction{ LmsApp->getDbSession().createReadTransaction() };

                const User::pointer user{ User::find(LmsApp->getDbSession(), valueText(LoginField).toUTF8()) };
                if (user)
                    error = Wt::WString::tr("Lms.Admin.User.user-already-exists");
            }
            else if (field == DemoField)
            {
                auto transaction{ LmsApp->getDbSession().createReadTransaction() };

                if (Wt::asNumber(value(DemoField)) && User::findDemoUser(LmsApp->getDbSession()))
                    error = Wt::WString::tr("Lms.Admin.User.demo-account-already-exists");
            }

            if (error.empty())
                return Wt::WFormModel::validateField(field);

            setValidation(field, Wt::WValidator::Result{ Wt::ValidationState::Invalid, error });

            return false;
        }

        std::optional<UserId> _userId;
        auth::IPasswordService* _authPasswordService{};
        auth::IAuthTokenService& _authTokenService;
    };

    UserView::UserView()
    {
        wApp->internalPathChanged().connect(this, [this]() {
            refreshView();
        });

        refreshView();
    }

    void UserView::refreshView()
    {
        if (!wApp->internalPathMatches("/admin/user"))
            return;

        const std::optional<UserId> userId{ core::stringUtils::readAs<UserId::ValueType>(wApp->internalPathNextPart("/admin/user/")) };

        clear();

        Wt::WTemplateFormView* t{ addNew<Wt::WTemplateFormView>(Wt::WString::tr("Lms.Admin.User.template")) };

        auth::IPasswordService* authPasswordService{};
        if (LmsApp->getAuthBackend() == AuthenticationBackend::Internal)
        {
            authPasswordService = core::Service<auth::IPasswordService>::get();
            assert(authPasswordService->canSetPasswords());
        }

        auto model{ std::make_shared<UserModel>(userId, authPasswordService, *core::Service<auth::IAuthTokenService>::get()) };
        if (userId)
        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };

            const User::pointer user{ User::find(LmsApp->getDbSession(), *userId) };
            if (!user)
                throw UserNotFoundException{};

            const Wt::WString title{ Wt::WString::tr("Lms.Admin.User.user-edit").arg(user->getLoginName()) };
            LmsApp->setTitle(title);

            t->bindString("title", title, Wt::TextFormat::Plain);
            t->setCondition("if-has-last-login", true);
            t->bindString("last-login", user->getLastLogin().toString(), Wt::TextFormat::Plain);
        }
        else
        {
            const Wt::WString title{ Wt::WString::tr("Lms.Admin.User.user-create") };
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
        if (!userId && core::Service<core::IConfig>::get()->getBool("demo", false))
            t->setCondition("if-demo", true);

        Wt::WPushButton* saveBtn{ t->bindNew<Wt::WPushButton>("save-btn", Wt::WString::tr(userId ? "Lms.save" : "Lms.create")) };
        saveBtn->clicked().connect([=]() {
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

} // namespace lms::ui
