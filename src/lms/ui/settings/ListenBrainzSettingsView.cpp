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

#include "ListenBrainzSettingsView.hpp"

#include <memory>
#include <string>

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
#include "services/feedback/IFeedbackService.hpp"
#include "services/scrobbling/IScrobblingService.hpp"

#include "LmsApplication.hpp"
#include "common/MandatoryValidator.hpp"

#include "SettingsViewUtils.hpp"

namespace lms::ui
{
    namespace
    {
        class ListenBrainzSettingsModel : public Wt::WFormModel
        {
        public:
            static inline const Field EnableScrobblingField{ "enable-scrobbling" };
            static inline const Field EnableFeedbackField{ "enable-feedback" };
            static inline const Field TokenField{ "token" };

            struct SaveResult
            {
                bool scrobblingTurnedOn{};
                bool feedbackTurnedOn{};
            };

            ListenBrainzSettingsModel()
            {
                addField(EnableScrobblingField);
                addField(EnableFeedbackField);
                addField(TokenField);

                setValidator(EnableScrobblingField, createMandatoryValidator());
                setValidator(EnableFeedbackField, createMandatoryValidator());
                setValidator(TokenField, createMandatoryValidator());

                loadData();
            }

            SaveResult saveData()
            {
                db::UserId userId;
                bool scrobblingTurnedOn{};
                bool feedbackTurnedOn{};

                {
                    auto transaction{ LmsApp->getDbSession().createWriteTransaction() };
                    db::User::pointer user{ LmsApp->getUser() };
                    userId = user->getId();

                    const bool wasScrobblingEnabled{ user->getScrobblingBackends().contains(db::ScrobblingBackend::ListenBrainz) };
                    const bool wasFeedbackEnabled{ user->getFeedbackBackends().contains(db::FeedbackBackend::ListenBrainz) };

                    const bool scrobblingEnabled{ Wt::asNumber(value(EnableScrobblingField)) != 0 };
                    const bool feedbackEnabled{ Wt::asNumber(value(EnableFeedbackField)) != 0 };

                    core::EnumSet<db::ScrobblingBackend> scrobblingBackends{ user->getScrobblingBackends() };
                    if (scrobblingEnabled)
                        scrobblingBackends.insert(db::ScrobblingBackend::ListenBrainz);
                    else
                        scrobblingBackends.erase(db::ScrobblingBackend::ListenBrainz);

                    user.modify()->setScrobblingBackends(scrobblingBackends);

                    core::EnumSet<db::FeedbackBackend> feedbackBackends{ user->getFeedbackBackends() };
                    if (feedbackEnabled)
                        feedbackBackends.insert(db::FeedbackBackend::ListenBrainz);
                    else
                        feedbackBackends.erase(db::FeedbackBackend::ListenBrainz);

                    user.modify()->setFeedbackBackends(feedbackBackends);

                    const std::string token{ Wt::asString(value(TokenField)).toUTF8() };
                    user.modify()->setListenBrainzToken(token);

                    scrobblingTurnedOn = !wasScrobblingEnabled && scrobblingEnabled && !token.empty();
                    feedbackTurnedOn = !wasFeedbackEnabled && feedbackEnabled && !token.empty();
                }

                if (scrobblingTurnedOn)
                    core::Service<scrobbling::IScrobblingService>::get()->requestImmediateExport(userId, db::ScrobblingBackend::ListenBrainz);

                if (feedbackTurnedOn)
                    core::Service<feedback::IFeedbackService>::get()->requestImmediateExport(userId, db::FeedbackBackend::ListenBrainz);

                return { scrobblingTurnedOn, feedbackTurnedOn };
            }

            void loadData()
            {
                auto transaction{ LmsApp->getDbSession().createReadTransaction() };
                const db::User::pointer user{ LmsApp->getUser() };

                const bool enableScrobbling{ user->getScrobblingBackends().contains(db::ScrobblingBackend::ListenBrainz) };
                const bool enableFeedback{ user->getFeedbackBackends().contains(db::FeedbackBackend::ListenBrainz) };

                setValue(EnableScrobblingField, enableScrobbling);
                setValue(EnableFeedbackField, enableFeedback);

                if (const auto token{ user->getListenBrainzToken() }; !token.empty())
                    setValue(TokenField, Wt::WString::fromUTF8(std::string{ token }));

                updateFieldStates(enableScrobbling, enableFeedback);
            }

            void updateFieldStates(bool enableScrobbling, bool enableFeedback)
            {
                const bool needsToken{ enableScrobbling || enableFeedback };
                setReadOnly(TokenField, !needsToken);
                validator(TokenField)->setMandatory(needsToken);
            }
        };
    } // namespace

    ListenBrainzSettingsView::ListenBrainzSettingsView()
    {
        wApp->internalPathChanged().connect(this, [this] {
            refreshView();
        });

        refreshView();
    }

    void ListenBrainzSettingsView::refreshView()
    {
        if (!wApp->internalPathMatches("/settings/listenbrainz"))
            return;

        clear();
        refreshForm();
        refreshImportCard();
    }

    void ListenBrainzSettingsView::refreshForm()
    {
        auto* t{ addNew<Wt::WTemplateFormView>(Wt::WString::tr("Lms.Settings.listenbrainz.template.form")) };
        auto model{ std::make_shared<ListenBrainzSettingsModel>() };

        t->setFormWidget(ListenBrainzSettingsModel::EnableScrobblingField, std::make_unique<Wt::WCheckBox>());
        t->setFormWidget(ListenBrainzSettingsModel::EnableFeedbackField, std::make_unique<Wt::WCheckBox>());

        {
            auto tokenEdit{ std::make_unique<Wt::WLineEdit>() };
            Wt::WLineEdit* tokenPtr{ tokenEdit.get() };
            tokenPtr->setEchoMode(Wt::EchoMode::Password);
            t->setFormWidget(ListenBrainzSettingsModel::TokenField, std::move(tokenEdit));

            auto visBtn{ std::make_unique<Wt::WPushButton>(Wt::WString::tr("Lms.template.toggle-visibility-btn"), Wt::TextFormat::XHTML) };
            visBtn->clicked().connect(this, [tokenPtr] {
                tokenPtr->setEchoMode(tokenPtr->echoMode() == Wt::EchoMode::Password ? Wt::EchoMode::Normal : Wt::EchoMode::Password);
            });
            t->bindWidget("token-visibility-btn", std::move(visBtn));
        }

        utils::bindSaveDiscardButtons(t, model.get(), [model, this] {
            const auto result{ model->saveData() };
            updateImportCardVisibility();
            if (result.scrobblingTurnedOn || result.feedbackTurnedOn)
                LmsApp->notifyMsg(Notification::Type::Info, Wt::WString::tr("Lms.Settings.export-started")); }, [model] { model->loadData(); });
        t->updateView(model.get());
    }

    void ListenBrainzSettingsView::refreshImportCard()
    {
        _importCard = addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Settings.listenbrainz.template.import-card"));
        _importCard->addFunction("tr", &Wt::WTemplate::Functions::tr);

        _importListensBtn = _importCard->bindNew<Wt::WPushButton>("import-listens-btn", Wt::WString::tr("Lms.Settings.listenbrainz-import-listens"));
        _importListensBtn->addStyleClass("btn btn-secondary");
        _importListensBtn->clicked().connect(this, [] {
            core::Service<scrobbling::IScrobblingService>::get()->requestImmediateImport(LmsApp->getUserId(), db::ScrobblingBackend::ListenBrainz);
            LmsApp->notifyMsg(Notification::Type::Info, Wt::WString::tr("Lms.Settings.listenbrainz-import-started"));
        });

        _importFeedbackBtn = _importCard->bindNew<Wt::WPushButton>("import-feedback-btn", Wt::WString::tr("Lms.Settings.listenbrainz-import-feedback"));
        _importFeedbackBtn->addStyleClass("btn btn-secondary");
        _importFeedbackBtn->clicked().connect(this, [] {
            core::Service<feedback::IFeedbackService>::get()->requestImmediateImport(LmsApp->getUserId(), db::FeedbackBackend::ListenBrainz);
            LmsApp->notifyMsg(Notification::Type::Info, Wt::WString::tr("Lms.Settings.listenbrainz-import-started"));
        });

        updateImportCardVisibility();
    }

    void ListenBrainzSettingsView::updateImportCardVisibility()
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };
        const db::User::pointer user{ LmsApp->getUser() };

        const bool scrobblingEnabled{ user->getScrobblingBackends().contains(db::ScrobblingBackend::ListenBrainz) };
        const bool feedbackEnabled{ user->getFeedbackBackends().contains(db::FeedbackBackend::ListenBrainz) };

        _importListensBtn->setHidden(!scrobblingEnabled);
        _importFeedbackBtn->setHidden(!feedbackEnabled);
        _importCard->setHidden(!scrobblingEnabled && !feedbackEnabled);
    }
} // namespace lms::ui
