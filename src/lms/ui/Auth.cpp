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

#include "Auth.hpp"

#include <Wt/Auth/HashFunction.h>

#include <Wt/WCheckBox.h>
#include <Wt/WEnvironment.h>
#include <Wt/WFormModel.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WRandom.h>

#include "core/Service.hpp"
#include "database/Session.hpp"
#include "database/objects/User.hpp"
#include "services/auth/IAuthTokenService.hpp"
#include "services/auth/IPasswordService.hpp"

#include "LmsApplication.hpp"
#include "common/LoginNameValidator.hpp"
#include "common/MandatoryValidator.hpp"
#include "common/PasswordValidator.hpp"

namespace lms::ui
{
    namespace
    {
        static constexpr core::LiteralString authTokenDomain{ "ui" };
        static const std::string authCookieName{ "LmsAuth" };
        static const std::string authCookieSalt{ Wt::Auth::SHA1HashFunction{}.compute(authCookieName, authTokenDomain.c_str()) }; // changing this will invalidate existing tokens

        void createAuthToken(db::UserId userId, const Wt::WDateTime& expiry)
        {
            const std::string authCookie{ Wt::WRandom::generateId(64) };
            const std::string hashedAuthCookie{ Wt::Auth::SHA1HashFunction{}.compute(authCookie, authCookieSalt) };
            core::Service<auth::IAuthTokenService>::get()->createAuthToken(authTokenDomain, userId, hashedAuthCookie);

            LmsApp->setCookie(authCookieName,
                authCookie,
                expiry.toTime_t() - Wt::WDateTime::currentDateTime().toTime_t(),
                "",
                "",
                LmsApp->environment().urlScheme() == "https");
        }

        class AuthModel : public Wt::WFormModel
        {
        public:
            // Associate each field with a unique string literal.
            static const Field LoginNameField;
            static const Field PasswordField;
            static const Field RememberMeField;

            AuthModel(auth::IPasswordService& passwordService)
                : _passwordService{ passwordService }
            {
                addField(LoginNameField);
                addField(PasswordField);
                addField(RememberMeField);

                setValidator(LoginNameField, createLoginNameValidator());
                setValidator(PasswordField, createMandatoryValidator());
            }

            void saveData()
            {
                bool isDemo;
                {
                    auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

                    db::User::pointer user{ db::User::find(LmsApp->getDbSession(), valueText(LoginNameField).toUTF8()) };
                    user.modify()->setLastLogin(Wt::WDateTime::currentDateTime());
                    _userId = user->getId();

                    isDemo = user->isDemo();
                }

                if (Wt::asNumber(value(RememberMeField)))
                {
                    const Wt::WDateTime now{ Wt::WDateTime::currentDateTime() };
                    createAuthToken(_userId, isDemo ? now.addDays(3) : now.addYears(1));
                }
            }

            bool validateField(Field field)
            {
                Wt::WString error;

                if (field == PasswordField)
                {
                    const auto checkResult{ _passwordService.checkUserPassword(
                        boost::asio::ip::make_address(LmsApp->environment().clientAddress()),
                        valueText(LoginNameField).toUTF8(),
                        valueText(PasswordField).toUTF8()) };
                    switch (checkResult.state)
                    {
                    case auth::IPasswordService::CheckResult::State::Granted:
                        _userId = checkResult.userId;
                        break;
                    case auth::IPasswordService::CheckResult::State::Denied:
                        error = Wt::WString::tr("Lms.password-bad-login-combination");
                        break;
                    case auth::IPasswordService::CheckResult::State::Throttled:
                        error = Wt::WString::tr("Lms.password-client-throttled");
                        break;
                    }
                }
                else
                {
                    return Wt::WFormModel::validateField(field);
                }

                setValidation(field, Wt::WValidator::Result(error.empty() ? Wt::ValidationState::Valid : Wt::ValidationState::Invalid, error));

                return (validation(field).state() == Wt::ValidationState::Valid);
            }

            db::UserId getUserId() const { return _userId; }

        private:
            db::UserId _userId;
            auth::IPasswordService& _passwordService;
        };

        const AuthModel::Field AuthModel::LoginNameField{ "login-name" };
        const AuthModel::Field AuthModel::PasswordField{ "password" };
        const AuthModel::Field AuthModel::RememberMeField{ "remember-me" };
    } // namespace

    db::UserId processAuthToken(const Wt::WEnvironment& env)
    {
        const std::string* authCookie{ env.getCookie(authCookieName) };
        if (!authCookie)
            return db::UserId{};

        const std::string hashedCookie{ Wt::Auth::SHA1HashFunction{}.compute(*authCookie, authCookieSalt) };

        const auto res{ core::Service<auth::IAuthTokenService>::get()->processAuthToken(authTokenDomain, boost::asio::ip::make_address(env.clientAddress()), hashedCookie) };
        switch (res.state)
        {
        case auth::IAuthTokenService::AuthTokenProcessResult::State::Denied:
        case auth::IAuthTokenService::AuthTokenProcessResult::State::Throttled:
            LmsApp->setCookie(authCookieName, std::string{}, 0, "", "", env.urlScheme() == "https");
            return db::UserId{};

        case auth::IAuthTokenService::AuthTokenProcessResult::State::Granted:
            assert(res.authTokenInfo->maxUseCount && res.authTokenInfo->maxUseCount.value() == 1); // single-use token
            createAuthToken(res.authTokenInfo->userId, res.authTokenInfo->expiry);
            break;
        }

        return res.authTokenInfo->userId;
    }

    void clearAuthTokens(db::UserId userId)
    {
        core::Service<auth::IAuthTokenService>::get()->clearAuthTokens(authTokenDomain, userId);
    }

    PasswordAuth::PasswordAuth(auth::IPasswordService& passwordService)
        : Wt::WTemplateFormView{ Wt::WString::tr("Lms.Auth.template") }
    {
        auto model{ std::make_shared<AuthModel>(passwordService) };

        auto processAuth = [this, model] {
            updateModel(model.get());

            if (model->validate())
            {
                model->saveData();
                assert(model->getUserId().isValid());
                userLoggedIn.emit(model->getUserId());
            }
            else
                updateView(model.get());
        };

        // LoginName
        auto loginName{ std::make_unique<Wt::WLineEdit>() };
        loginName->setAttributeValue("autocomplete", "username");
        setFormWidget(AuthModel::LoginNameField, std::move(loginName));

        // Password
        auto password = std::make_unique<Wt::WLineEdit>();
        password->setEchoMode(Wt::EchoMode::Password);
        password->enterPressed().connect(this, processAuth);
        password->setAttributeValue("autocomplete", "current-password");
        setFormWidget(AuthModel::PasswordField, std::move(password));

        // Remember me
        setFormWidget(AuthModel::RememberMeField, std::make_unique<Wt::WCheckBox>());

        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };

            db::User::pointer demoUser{ db::User::findDemoUser(LmsApp->getDbSession()) };
            if (demoUser)
            {
                model->setValue(AuthModel::LoginNameField, demoUser->getLoginName());
                model->setValue(AuthModel::PasswordField, demoUser->getLoginName());
            }
        }

        Wt::WPushButton* loginBtn{ bindNew<Wt::WPushButton>("login-btn", Wt::WString::tr("Lms.login")) };
        loginBtn->clicked().connect(this, processAuth);

        updateView(model.get());
    }
} // namespace lms::ui
