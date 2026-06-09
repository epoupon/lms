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

#include "ServicesSettingsView.hpp"

#include <memory>
#include <string>

#include <Wt/WAnchor.h>
#include <Wt/WComboBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WFormModel.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WString.h>
#include <Wt/WTemplate.h>
#include <Wt/WTemplateFormView.h>
#include <Wt/WText.h>

#include "core/Service.hpp"
#include "database/Session.hpp"
#include "database/objects/User.hpp"
#include "services/scrobbling/IScrobblingService.hpp"

#include "LmsApplication.hpp"
#include "ModalManager.hpp"
#include "common/MandatoryValidator.hpp"
#include "common/ValueStringModel.hpp"

#include "SettingsViewUtils.hpp"

namespace lms::ui
{
    namespace
    {
        class ServicesSettingsModel : public Wt::WFormModel
        {
        public:
            static inline const Field FeedbackBackendField{ "feedback-backend" };
            static inline const Field ScrobblingBackendField{ "scrobbling-backend" };
            static inline const Field ListenBrainzTokenField{ "listenbrainz-token" };

            using FeedbackBackendModel = ValueStringModel<db::FeedbackBackend>;
            using ScrobblingBackendModel = ValueStringModel<db::ScrobblingBackend>;

            ServicesSettingsModel()
            {
                _feedbackBackendModel = std::make_shared<FeedbackBackendModel>();
                _feedbackBackendModel->add(Wt::WString::tr("Lms.Settings.backend.internal"), db::FeedbackBackend::Internal);
                _feedbackBackendModel->add(Wt::WString::tr("Lms.Settings.backend.listenbrainz"), db::FeedbackBackend::ListenBrainz);

                _scrobblingBackendModel = std::make_shared<ScrobblingBackendModel>();
                _scrobblingBackendModel->add(Wt::WString::tr("Lms.Settings.backend.internal"), db::ScrobblingBackend::Internal);
                _scrobblingBackendModel->add(Wt::WString::tr("Lms.Settings.backend.listenbrainz"), db::ScrobblingBackend::ListenBrainz);
                _scrobblingBackendModel->add(Wt::WString::tr("Lms.Settings.backend.lastfm"), db::ScrobblingBackend::LastFm);

                addField(FeedbackBackendField);
                addField(ScrobblingBackendField);
                addField(ListenBrainzTokenField);

                setValidator(ListenBrainzTokenField, createMandatoryValidator());

                loadData();
            }

            std::shared_ptr<FeedbackBackendModel> getFeedbackBackendModel() { return _feedbackBackendModel; }
            std::shared_ptr<ScrobblingBackendModel> getScrobblingBackendModel() { return _scrobblingBackendModel; }

            void saveData()
            {
                auto transaction{ LmsApp->getDbSession().createWriteTransaction() };
                db::User::pointer user{ LmsApp->getUser() };

                if (auto row{ _feedbackBackendModel->getRowFromString(valueText(FeedbackBackendField)) })
                    user.modify()->setFeedbackBackend(_feedbackBackendModel->getValue(*row));

                if (auto row{ _scrobblingBackendModel->getRowFromString(valueText(ScrobblingBackendField)) })
                    user.modify()->setScrobblingBackend(_scrobblingBackendModel->getValue(*row));

                user.modify()->setListenBrainzToken(Wt::asString(value(ListenBrainzTokenField)).toUTF8());
            }

            void loadData()
            {
                auto transaction{ LmsApp->getDbSession().createReadTransaction() };
                const db::User::pointer user{ LmsApp->getUser() };

                if (auto row{ _feedbackBackendModel->getRowFromValue(user->getFeedbackBackend()) })
                    setValue(FeedbackBackendField, _feedbackBackendModel->getString(*row));

                if (auto row{ _scrobblingBackendModel->getRowFromValue(user->getScrobblingBackend()) })
                    setValue(ScrobblingBackendField, _scrobblingBackendModel->getString(*row));

                if (const auto token{ user->getListenBrainzToken() }; !token.empty())
                    setValue(ListenBrainzTokenField, Wt::WString::fromUTF8(std::string{ token }));

                updateFieldStates(user->getScrobblingBackend(), user->getFeedbackBackend());
            }

            void updateFieldStates(db::ScrobblingBackend scrobblingBackend, db::FeedbackBackend feedbackBackend)
            {
                const bool usesListenBrainz{ scrobblingBackend == db::ScrobblingBackend::ListenBrainz || feedbackBackend == db::FeedbackBackend::ListenBrainz };
                setReadOnly(ListenBrainzTokenField, !usesListenBrainz);
                validator(ListenBrainzTokenField)->setMandatory(usesListenBrainz);
            }

        private:
            std::shared_ptr<FeedbackBackendModel> _feedbackBackendModel;
            std::shared_ptr<ScrobblingBackendModel> _scrobblingBackendModel;
        };
    } // namespace

    ServicesSettingsView::ServicesSettingsView()
    {
        wApp->internalPathChanged().connect(this, [this] {
            refreshView();
        });

        refreshView();
    }

    void ServicesSettingsView::refreshView()
    {
        if (!wApp->internalPathMatches("/settings/services"))
            return;

        clear();
        refreshFormSection();
        refreshLastFmCardSection();
    }

    void ServicesSettingsView::refreshFormSection()
    {
        auto* t{ addNew<Wt::WTemplateFormView>(Wt::WString::tr("Lms.Settings.services.template")) };
        auto model{ std::make_shared<ServicesSettingsModel>() };

        Wt::WComboBox* feedbackBackendRaw{};
        {
            auto feedbackBackend{ std::make_unique<Wt::WComboBox>() };
            feedbackBackend->setModel(model->getFeedbackBackendModel());
            feedbackBackendRaw = feedbackBackend.get();
            t->setFormWidget(ServicesSettingsModel::FeedbackBackendField, std::move(feedbackBackend));
        }

        Wt::WComboBox* scrobblingBackendRaw{};
        {
            auto scrobblingBackend{ std::make_unique<Wt::WComboBox>() };
            scrobblingBackend->setModel(model->getScrobblingBackendModel());
            scrobblingBackendRaw = scrobblingBackend.get();
            t->setFormWidget(ServicesSettingsModel::ScrobblingBackendField, std::move(scrobblingBackend));
        }

        {
            auto tokenEdit{ std::make_unique<Wt::WLineEdit>() };
            Wt::WLineEdit* tokenPtr{ tokenEdit.get() };
            tokenPtr->setEchoMode(Wt::EchoMode::Password);
            t->setFormWidget(ServicesSettingsModel::ListenBrainzTokenField, std::move(tokenEdit));

            auto visBtn{ std::make_unique<Wt::WPushButton>(Wt::WString::tr("Lms.template.toggle-visibility-btn"), Wt::TextFormat::XHTML) };
            visBtn->clicked().connect(this, [tokenPtr] {
                tokenPtr->setEchoMode(tokenPtr->echoMode() == Wt::EchoMode::Password ? Wt::EchoMode::Normal : Wt::EchoMode::Password);
            });
            t->bindWidget("listenbrainz-token-visibility-btn", std::move(visBtn));
        }

        auto updateFieldStates{ [=] {
            const db::ScrobblingBackend scrobBackend{ model->getScrobblingBackendModel()->getValue(scrobblingBackendRaw->currentIndex()) };
            const db::FeedbackBackend feedBackend{ model->getFeedbackBackendModel()->getValue(feedbackBackendRaw->currentIndex()) };
            model->updateFieldStates(scrobBackend, feedBackend);
            t->updateModel(model.get());
            t->updateView(model.get());
        } };

        feedbackBackendRaw->activated().connect([=] { updateFieldStates(); });
        scrobblingBackendRaw->activated().connect([=] { updateFieldStates(); });

        utils::bindSaveDiscardButtons(t, model.get(), [model, this] { model->saveData(); refreshView(); }, [model] { model->loadData(); });
        t->updateView(model.get());
    }

    void ServicesSettingsView::refreshLastFmCardSection()
    {
        auto* lastFmCard{ addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Settings.services.lastfm.template.card")) };
        lastFmCard->addFunction("tr", &Wt::WTemplate::Functions::tr);

        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };
            lastFmCard->setHidden(LmsApp->getUser()->getScrobblingBackend() != db::ScrobblingBackend::LastFm);
        }

        auto* cardContent{ lastFmCard->bindNew<Wt::WContainerWidget>("card-content") };
        cardContent->addStyleClass("d-flex align-items-center gap-2");

        const bool linked{ [&] {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };
            return !LmsApp->getUser()->getLastFmSessionKey().empty();
        }() };

        if (linked)
        {
            lastFmCard->bindNew<Wt::WText>("card-header-badge", "<span class=\"badge text-bg-success\">" + Wt::WString::tr("Lms.Settings.services.lastfm-linked").toUTF8() + "</span>", Wt::TextFormat::UnsafeXHTML);
            auto* unlinkBtn{ cardContent->addNew<Wt::WPushButton>(Wt::WString::tr("Lms.Settings.services.lastfm-unlink")) };
            unlinkBtn->addStyleClass("btn btn-secondary");
            unlinkBtn->clicked().connect(this, [this] {
                auto modal{ std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.Settings.services.lastfm.template.unlink-confirm")) };
                modal->addFunction("tr", &Wt::WTemplate::Functions::tr);
                Wt::WTemplate* modalPtr{ modal.get() };
                modal->bindNew<Wt::WPushButton>("confirm-btn", Wt::WString::tr("Lms.Settings.services.lastfm-unlink"))
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
            lastFmCard->bindNew<Wt::WContainerWidget>("card-header-badge");
            auto* linkBtn{ cardContent->addNew<Wt::WPushButton>(Wt::WString::tr("Lms.Settings.services.lastfm-link")) };
            linkBtn->addStyleClass("btn btn-secondary");
            linkBtn->clicked().connect(this, [this] { showLastFmLinkModal(); });
        }
    }

    void ServicesSettingsView::showLastFmLinkModal()
    {
        auto modal{ std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.Settings.services.lastfm.template.link-modal")) };
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

        auto* authorizeBtn{ modal->bindNew<Wt::WPushButton>("authorize-btn", Wt::WString::tr("Lms.Settings.services.lastfm-authorize")) };
        auto* doneBtn{ modal->bindNew<Wt::WPushButton>("done-btn", Wt::WString::tr("Lms.Settings.services.lastfm-done")) };
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
                LmsApp->notifyMsg(Notification::Type::Warning, Wt::WString::tr("Lms.Settings.services.lastfm-missing-credentials"));
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
                        authAnchorContainer->addNew<Wt::WAnchor>(link, Wt::WString::tr("Lms.Settings.services.lastfm-auth-url"));
                        authAnchorContainer->show();
                        authorizeBtn->hide();
                        doneBtn->show();
                        wApp->triggerUpdate();
                    });
                },
                [sessionId] {
                    LmsApplication::post(sessionId, [] {
                        LmsApp->notifyMsg(Notification::Type::Warning, Wt::WString::tr("Lms.Settings.services.lastfm-auth-error"));
                        wApp->triggerUpdate();
                    });
                });
        });

        doneBtn->clicked().connect([=, this] {
            const std::string sessionId{ wApp->sessionId() };
            const db::UserId userId{ LmsApp->getUserId() };

            core::Service<scrobbling::IScrobblingService>::get()->continueLastFmLink(
                userId,
                [sessionId, modalPtr, this] {
                    LmsApplication::post(sessionId, [sessionId, modalPtr, this] {
                        LmsApp->getModalManager().dispose(modalPtr);
                        refreshView();
                        wApp->triggerUpdate();
                    });
                },
                [sessionId] {
                    LmsApplication::post(sessionId, [] {
                        LmsApp->notifyMsg(Notification::Type::Warning, Wt::WString::tr("Lms.Settings.services.lastfm-auth-error"));
                        wApp->triggerUpdate();
                    });
                });
        });

        LmsApp->getModalManager().show(std::move(modal));
    }
} // namespace lms::ui
