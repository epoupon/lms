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

#include "LastFmSettingsView.hpp"

#include <memory>
#include <string>

#include <Wt/WAnchor.h>
#include <Wt/WCheckBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WFormModel.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WString.h>
#include <Wt/WTemplate.h>
#include <Wt/WTemplateFormView.h>

#include "core/EnumSet.hpp"
#include "core/Service.hpp"
#include "database/Session.hpp"
#include "database/objects/User.hpp"
#include "services/scrobbling/IScrobblingService.hpp"

#include "LmsApplication.hpp"
#include "ModalManager.hpp"

#include "SettingsViewUtils.hpp"

namespace lms::ui
{
    namespace
    {
        class LastFmSettingsModel : public Wt::WFormModel
        {
        public:
            static inline const Field EnableScrobblingField{ "enable-scrobbling" };

            LastFmSettingsModel()
            {
                addField(EnableScrobblingField);

                loadData();
            }

            void saveData()
            {
                auto transaction{ LmsApp->getDbSession().createWriteTransaction() };
                db::User::pointer user{ LmsApp->getUser() };

                core::EnumSet<db::ScrobblingBackend> scrobblingBackends{ user->getScrobblingBackends() };
                if (Wt::asNumber(value(EnableScrobblingField)) != 0)
                    scrobblingBackends.insert(db::ScrobblingBackend::LastFm);
                else
                    scrobblingBackends.erase(db::ScrobblingBackend::LastFm);
                user.modify()->setScrobblingBackends(scrobblingBackends);
            }

            void loadData()
            {
                auto transaction{ LmsApp->getDbSession().createReadTransaction() };
                const db::User::pointer user{ LmsApp->getUser() };

                setValue(EnableScrobblingField, user->getScrobblingBackends().contains(db::ScrobblingBackend::LastFm));
            }
        };
    } // namespace

    LastFmSettingsView::LastFmSettingsView()
    {
        wApp->internalPathChanged().connect(this, [this] {
            refreshView();
        });

        refreshView();
    }

    void LastFmSettingsView::refreshView()
    {
        if (!wApp->internalPathMatches("/settings/lastfm"))
            return;

        clear();
        refreshForm();
        refreshAccountCard();
    }

    void LastFmSettingsView::refreshForm()
    {
        auto* t{ addNew<Wt::WTemplateFormView>(Wt::WString::tr("Lms.Settings.lastfm.template.form")) };
        auto model{ std::make_shared<LastFmSettingsModel>() };

        t->setFormWidget(LastFmSettingsModel::EnableScrobblingField, std::make_unique<Wt::WCheckBox>());

        utils::bindSaveDiscardButtons(t, model.get(), [model, this] { 
            model->saveData(); updateAccountCardVisibility(); }, [model] { model->loadData(); });
        t->updateView(model.get());
    }

    void LastFmSettingsView::updateAccountCardVisibility()
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };
        _accountCard->setHidden(!LmsApp->getUser()->getScrobblingBackends().contains(db::ScrobblingBackend::LastFm));
    }

    void LastFmSettingsView::refreshAccountCard()
    {
        _accountCard = addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Settings.lastfm.template.card"));
        Wt::WTemplate* lastFmCard{ _accountCard };
        lastFmCard->addFunction("tr", &Wt::WTemplate::Functions::tr);

        updateAccountCardVisibility();

        const bool linked{ [&] {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };
            return !LmsApp->getUser()->getLastFmSessionKey().empty();
        }() };

        lastFmCard->setCondition("if-linked", linked);
        lastFmCard->setCondition("if-unlinked", !linked);

        if (linked)
        {
            auto* unlinkBtn{ lastFmCard->bindNew<Wt::WPushButton>("unlink-btn", Wt::WString::tr("Lms.Settings.lastfm-unlink")) };
            unlinkBtn->clicked().connect(this, [this] {
                auto modal{ std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.Settings.lastfm.template.unlink-confirm")) };
                modal->addFunction("tr", &Wt::WTemplate::Functions::tr);
                Wt::WTemplate* modalPtr{ modal.get() };
                modal->bindNew<Wt::WPushButton>("confirm-btn", Wt::WString::tr("Lms.Settings.lastfm-unlink"))
                    ->clicked()
                    .connect(this, [this, modalPtr] {
                        {
                            auto transaction{ LmsApp->getDbSession().createWriteTransaction() };
                            LmsApp->getUser().modify()->setLastFmSessionKey("");
                        }
                        LmsApp->getModalManager().dispose(modalPtr);
                        refreshView();
                    });
                modal->bindNew<Wt::WPushButton>("cancel-btn", Wt::WString::tr("Lms.cancel"))
                    ->clicked()
                    .connect([modalPtr] { LmsApp->getModalManager().dispose(modalPtr); });
                LmsApp->getModalManager().show(std::move(modal));
            });
        }
        else
        {
            auto* linkBtn{ lastFmCard->bindNew<Wt::WPushButton>("link-btn", Wt::WString::tr("Lms.Settings.lastfm-link")) };
            linkBtn->clicked().connect(this, [this] { showLinkModal(); });
        }
    }

    void LastFmSettingsView::showLinkModal()
    {
        auto modal{ std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.Settings.lastfm.template.link-modal")) };
        modal->addFunction("tr", &Wt::WTemplate::Functions::tr);
        Wt::WTemplate* modalPtr{ modal.get() };

        auto* apiKeyEdit{ modal->bindNew<Wt::WLineEdit>("api-key") };
        auto* apiSecretEdit{ modal->bindNew<Wt::WLineEdit>("api-secret") };
        apiSecretEdit->setEchoMode(Wt::EchoMode::Password);

        // pre-fill from DB if credentials already exist
        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };
            const db::User::pointer user{ LmsApp->getUser() };
            if (const auto key{ user->getLastFmApiKey() }; !key.empty())
                apiKeyEdit->setValueText(Wt::WString::fromUTF8(std::string{ key }));
            if (const auto secret{ user->getLastFmApiSecret() }; !secret.empty())
                apiSecretEdit->setValueText(Wt::WString::fromUTF8(std::string{ secret }));
        }

        auto* visBtn{ modal->bindNew<Wt::WPushButton>("api-secret-visibility-btn", Wt::WString::tr("Lms.template.toggle-visibility-btn"), Wt::TextFormat::XHTML) };
        visBtn->clicked().connect([apiSecretEdit] {
            apiSecretEdit->setEchoMode(apiSecretEdit->echoMode() == Wt::EchoMode::Password ? Wt::EchoMode::Normal : Wt::EchoMode::Password);
        });

        auto* authAnchorContainer{ modal->bindNew<Wt::WContainerWidget>("auth-anchor-container") };
        authAnchorContainer->hide();

        auto* authorizeBtn{ modal->bindNew<Wt::WPushButton>("authorize-btn", Wt::WString::tr("Lms.Settings.lastfm-authorize")) };
        auto* doneBtn{ modal->bindNew<Wt::WPushButton>("done-btn", Wt::WString::tr("Lms.Settings.lastfm-done")) };
        doneBtn->hide();

        auto* cancelBtn{ modal->bindNew<Wt::WPushButton>("cancel-btn", Wt::WString::tr("Lms.cancel")) };
        cancelBtn->clicked().connect([modalPtr] {
            LmsApp->getModalManager().dispose(modalPtr);
        });

        authorizeBtn->clicked().connect([=] {
            const std::string apiKey{ apiKeyEdit->valueText().toUTF8() };
            const std::string apiSecret{ apiSecretEdit->valueText().toUTF8() };

            if (apiKey.empty() || apiSecret.empty())
            {
                LmsApp->notifyMsg(Notification::Type::Warning, Wt::WString::tr("Lms.Settings.lastfm-missing-credentials"));
                return;
            }

            const std::string sessionId{ wApp->sessionId() };
            const db::UserId userId{ LmsApp->getUserId() };

            core::Service<scrobbling::IScrobblingService>::get()->initiateLastFmLink(
                userId, apiKey, apiSecret,
                [sessionId, authAnchorContainer, authorizeBtn, doneBtn](std::string_view authUrl) {
                    LmsApplication::post(sessionId, [=, url = std::string{ authUrl }] {
                        wApp->doJavaScript("window.open('" + url + "', '_blank');");
                        Wt::WLink link{ url };
                        link.setTarget(Wt::LinkTarget::NewWindow);
                        authAnchorContainer->addNew<Wt::WAnchor>(link, Wt::WString::tr("Lms.Settings.lastfm-auth-url"));
                        authAnchorContainer->show();
                        authorizeBtn->hide();
                        doneBtn->show();
                        wApp->triggerUpdate();
                    });
                },
                [sessionId] {
                    LmsApplication::post(sessionId, [] {
                        LmsApp->notifyMsg(Notification::Type::Warning, Wt::WString::tr("Lms.Settings.lastfm-auth-error"));
                        wApp->triggerUpdate();
                    });
                });
        });

        doneBtn->clicked().connect([=, this] {
            const std::string sessionId{ wApp->sessionId() };
            const db::UserId userId{ LmsApp->getUserId() };

            core::Service<scrobbling::IScrobblingService>::get()->continueLastFmLink(
                userId,
                [sessionId, userId, modalPtr, this] {
                    core::Service<scrobbling::IScrobblingService>::get()->requestImmediateExport(userId, db::ScrobblingBackend::LastFm);
                    LmsApplication::post(sessionId, [sessionId, modalPtr, this] {
                        LmsApp->getModalManager().dispose(modalPtr);
                        refreshView();
                        wApp->triggerUpdate();
                        LmsApp->notifyMsg(Notification::Type::Info, Wt::WString::tr("Lms.Settings.export-started"));
                    });
                },
                [sessionId] {
                    LmsApplication::post(sessionId, [] {
                        LmsApp->notifyMsg(Notification::Type::Warning, Wt::WString::tr("Lms.Settings.lastfm-auth-error"));
                        wApp->triggerUpdate();
                    });
                });
        });

        LmsApp->getModalManager().show(std::move(modal));
    }
} // namespace lms::ui
