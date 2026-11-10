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

#include "PasswordSettingsView.hpp"

#include <Wt/WFormModel.h>
#include <Wt/WLineEdit.h>
#include <Wt/WString.h>
#include <Wt/WTemplateFormView.h>

#include "core/Service.hpp"

#include "database/ISession.hpp"
#include "database/objects/User.hpp"
#include "services/auth/IAuthTokenService.hpp"
#include "services/auth/IPasswordService.hpp"

#include "LmsApplication.hpp"
#include "SettingsViewUtils.hpp"
#include "common/PasswordValidator.hpp"

namespace lms::ui
{
    namespace
    {
        class PasswordSettingsModel : public Wt::WFormModel
        {
        public:
            static inline const Field PasswordOldField{ "password-old" };
            static inline const Field PasswordField{ "password" };
            static inline const Field PasswordConfirmField{ "password-confirm" };

            PasswordSettingsModel(auth::IPasswordService& authPasswordService, bool withOldPassword, auth::IAuthTokenService& authTokenService)
                : _authPasswordService{ authPasswordService }
                , _withOldPassword{ withOldPassword }
                , _authTokenService{ authTokenService }
            {
                if (_withOldPassword)
                {
                    addField(PasswordOldField);
                    setValidator(PasswordOldField, createPasswordCheckValidator(_authPasswordService));
                }

                addField(PasswordField);
                setValidator(PasswordField, createPasswordStrengthValidator(_authPasswordService, [] { return auth::PasswordValidationContext{ .loginName = std::string{ LmsApp->getUserLoginName() }, .userType = LmsApp->getUserType() }; }));
                addField(PasswordConfirmField);

                loadData();
            }

            void saveData()
            {
                if (!valueText(PasswordField).empty())
                {
                    auto transaction{ LmsApp->getDbSession().createWriteTransaction() };
                    const db::User user{ LmsApp->getUser() };
                    _authPasswordService.setPassword(user.getId(), valueText(PasswordField).toUTF8());
                    _authTokenService.clearAuthTokens("ui", user.getId());
                }
            }

            void loadData()
            {
                if (_withOldPassword)
                    setValue(PasswordOldField, "");
                setValue(PasswordField, "");
                setValue(PasswordConfirmField, "");
            }

        private:
            bool validateField(Field field)
            {
                Wt::WString error;

                if (field == PasswordOldField)
                {
                    if (valueText(PasswordOldField).empty() && !valueText(PasswordField).empty())
                        error = Wt::WString::tr("Lms.Settings.password-must-fill-old-password");
                    else
                        return Wt::WFormModel::validateField(field);
                }
                else if (field == PasswordField)
                {
                    if (!valueText(PasswordOldField).empty() && valueText(PasswordField).empty())
                        error = Wt::WString::tr("Wt.WValidator.Invalid");
                    else
                        return Wt::WFormModel::validateField(field);
                }
                else if (field == PasswordConfirmField)
                {
                    if (validation(PasswordField).state() == Wt::ValidationState::Valid)
                    {
                        if (valueText(PasswordField) != valueText(PasswordConfirmField))
                            error = Wt::WString::tr("Lms.passwords-dont-match");
                    }
                }
                else
                {
                    return Wt::WFormModel::validateField(field);
                }

                setValidation(field, Wt::WValidator::Result(error.empty() ? Wt::ValidationState::Valid : Wt::ValidationState::Invalid, error));
                return (validation(field).state() == Wt::ValidationState::Valid);
            }

            auth::IPasswordService& _authPasswordService;
            bool _withOldPassword{};
            auth::IAuthTokenService& _authTokenService;
        };
    } // namespace

    PasswordSettingsView::PasswordSettingsView()
    {
        wApp->internalPathChanged().connect(this, [this] {
            refreshView();
        });

        refreshView();
    }

    void PasswordSettingsView::refreshView()
    {
        if (!wApp->internalPathMatches("/settings/password"))
            return;

        clear();

        if (LmsApp->getAuthBackend() != AuthenticationBackend::Internal)
            return;

        auth::IPasswordService* authPasswordService{ core::Service<auth::IPasswordService>::get() };
        if (!authPasswordService)
            return;

        const bool withOldPassword{ !LmsApp->isUserAuthStrong() };

        auto* t{ addNew<Wt::WTemplateFormView>(Wt::WString::tr("Lms.Settings.password.template")) };

        if (withOldPassword)
        {
            t->setCondition("if-has-old-password", true);
            auto oldPassword{ std::make_unique<Wt::WLineEdit>() };
            oldPassword->setEchoMode(Wt::EchoMode::Password);
            oldPassword->setAttributeValue("autocomplete", "current-password");
            t->setFormWidget(PasswordSettingsModel::PasswordOldField, std::move(oldPassword));
        }

        auto password{ std::make_unique<Wt::WLineEdit>() };
        password->setEchoMode(Wt::EchoMode::Password);
        password->setAttributeValue("autocomplete", "new-password");
        t->setFormWidget(PasswordSettingsModel::PasswordField, std::move(password));

        auto passwordConfirm{ std::make_unique<Wt::WLineEdit>() };
        passwordConfirm->setEchoMode(Wt::EchoMode::Password);
        passwordConfirm->setAttributeValue("autocomplete", "new-password");
        t->setFormWidget(PasswordSettingsModel::PasswordConfirmField, std::move(passwordConfirm));

        auto model{ std::make_shared<PasswordSettingsModel>(*authPasswordService, withOldPassword, *core::Service<auth::IAuthTokenService>::get()) };

        utils::bindSaveDiscardButtons(t, model.get(), [model] { model->saveData(); }, [model] { model->loadData(); });
        t->updateView(model.get());
    }
} // namespace lms::ui
