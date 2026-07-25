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

            void saveData()
            {
                auto transaction{ LmsApp->getDbSession().createWriteTransaction() };
                db::User::pointer user{ LmsApp->getUser() };

                core::EnumSet<db::ScrobblingBackend> scrobblingBackends{ user->getScrobblingBackends() };

                if (Wt::asNumber(value(EnableScrobblingField)) != 0)
                    scrobblingBackends.insert(db::ScrobblingBackend::ListenBrainz);
                else
                    scrobblingBackends.erase(db::ScrobblingBackend::ListenBrainz);

                user.modify()->setScrobblingBackends(scrobblingBackends);

                const bool enableFeedback{ Wt::asNumber(value(EnableFeedbackField)) != 0 };
                user.modify()->setFeedbackBackend(enableFeedback ? db::FeedbackBackend::ListenBrainz : db::FeedbackBackend::Internal);

                user.modify()->setListenBrainzToken(Wt::asString(value(TokenField)).toUTF8());
            }

            void loadData()
            {
                auto transaction{ LmsApp->getDbSession().createReadTransaction() };
                const db::User::pointer user{ LmsApp->getUser() };

                const bool enableScrobbling{ user->getScrobblingBackends().contains(db::ScrobblingBackend::ListenBrainz) };
                const bool enableFeedback{ user->getFeedbackBackend() == db::FeedbackBackend::ListenBrainz };

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
             model->saveData(); updateImportCardVisibility(); }, [model] { model->loadData(); });
        t->updateView(model.get());
    }

    void ListenBrainzSettingsView::refreshImportCard()
    {
        _importCard = addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Settings.listenbrainz.template.import-card"));
        _importCard->addFunction("tr", &Wt::WTemplate::Functions::tr);

        updateImportCardVisibility();

        auto* importBtn{ _importCard->bindNew<Wt::WPushButton>("import-btn", Wt::WString::tr("Lms.Settings.listenbrainz-import")) };
        importBtn->addStyleClass("btn btn-secondary");
        importBtn->clicked().connect(this, [] {
            core::Service<scrobbling::IScrobblingService>::get()->requestImmediateImport(LmsApp->getUserId(), db::ScrobblingBackend::ListenBrainz);
            LmsApp->notifyMsg(Notification::Type::Info, Wt::WString::tr("Lms.Settings.listenbrainz-import-started"));
        });
    }

    void ListenBrainzSettingsView::updateImportCardVisibility()
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };
        _importCard->setHidden(!LmsApp->getUser()->getScrobblingBackends().contains(db::ScrobblingBackend::ListenBrainz));
    }
} // namespace lms::ui
