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

#include "SettingsViewUtils.hpp"

#include <functional>
#include <memory>

#include <Wt/WButtonGroup.h>
#include <Wt/WDateEdit.h>
#include <Wt/WLocalDateTime.h>
#include <Wt/WPushButton.h>
#include <Wt/WRadioButton.h>
#include <Wt/WString.h>
#include <Wt/WTemplate.h>
#include <Wt/WText.h>
#include <Wt/WTime.h>

#include "database/ISession.hpp"
#include "database/objects/Listen.hpp"
#include "database/objects/ListenBackendSync.hpp"
#include "database/objects/TrackFeedbackBackendSync.hpp"

#include "LmsApplication.hpp"
#include "ModalManager.hpp"

namespace lms::ui::utils
{
    namespace
    {
        // the user picks a day in their own time zone, but listens are stored in UTC
        Wt::WDateTime toUTCStartOfDay(const Wt::WDate& date, const Wt::WLocale& locale)
        {
            if (!locale.timeZone())
                return Wt::WDateTime{ date };

            // a DST change can skip or repeat midnight: without a choice, Wt makes such a time invalid, which would mean no lower bound
            Wt::WLocalDateTime startOfDay{ locale };
            startOfDay.setDateTime(date, Wt::WTime{ 0, 0 }, false /* earliest */);
            return startOfDay.toUTC();
        }

        Wt::WString getListenCountText(const Wt::WDateTime& since)
        {
            std::size_t count{};
            {
                auto transaction{ LmsApp->getDbSession().createReadTransaction() };
                count = db::Listen::getCount(LmsApp->getDbSession(), db::Listen::FindParameters{}.setUser(LmsApp->getUserId()).setMinDateTime(since));
            }

            return Wt::WString::trn("Lms.Settings.export-listens-count", count).arg(count);
        }
    } // namespace

    void bindSaveDiscardButtons(Wt::WTemplateFormView* t, Wt::WFormModel* model, const std::function<bool()>& saveData, const std::function<void()>& loadData)
    {
        Wt::WPushButton* saveBtn{ t->bindWidget("save-btn", std::make_unique<Wt::WPushButton>(Wt::WString::tr("Lms.save"))) };
        Wt::WPushButton* discardBtn{ t->bindWidget("discard-btn", std::make_unique<Wt::WPushButton>(Wt::WString::tr("Lms.discard"))) };

        saveBtn->clicked().connect([=] {
            if (LmsApp->getUserType() == db::UserType::DEMO)
            {
                LmsApp->notifyMsg(notification::Type::Warning, Wt::WString::tr("Lms.Settings.demo-cannot-save"));
                return;
            }
            t->updateModel(model);
            if (model->validate())
            {
                if (saveData())
                    LmsApp->notifyMsg(notification::Type::Info, Wt::WString::tr("Lms.Settings.settings-saved"));
            }
            t->updateView(model);
        });

        discardBtn->clicked().connect([=] {
            loadData();
            model->validate();
            t->updateView(model);
        });
    }

    void showExportListensModal(const Wt::WString& backendName, std::function<void(const Wt::WDateTime& exportSince)> onSave)
    {
        auto modal{ std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.Settings.template.export-listens")) };
        modal->addFunction("id", &Wt::WTemplate::Functions::id);
        modal->addFunction("tr", &Wt::WTemplate::Functions::tr);
        Wt::WWidget* modalPtr{ modal.get() };

        modal->bindString("desc", Wt::WString::tr("Lms.Settings.export-listens-desc").arg(backendName), Wt::TextFormat::Plain);

        const Wt::WLocale& locale{ LmsApp->locale() };
        const Wt::WDate today{ locale.timeZone() ? Wt::WLocalDateTime::currentDateTime(locale).date() : Wt::WDate::currentDate() };

        Wt::WButtonGroup* exportRange{ modal->addChild(std::make_unique<Wt::WButtonGroup>()) };
        Wt::WRadioButton* allBtn{ modal->bindNew<Wt::WRadioButton>("all-btn") };
        Wt::WRadioButton* sinceBtn{ modal->bindNew<Wt::WRadioButton>("since-btn") };
        exportRange->addButton(allBtn);
        exportRange->addButton(sinceBtn);
        allBtn->setChecked();

        modal->bindNew<Wt::WText>("all-count", getListenCountText(Wt::WDateTime{}), Wt::TextFormat::Plain);

        Wt::WDateEdit* sinceDate{ modal->bindNew<Wt::WDateEdit>("since-date") };
        sinceDate->setNativeControl(true);
        sinceDate->setDate(today);
        sinceDate->setTop(today);
        sinceDate->validator()->setMandatory(true);
        sinceDate->setDisabled(true);
        exportRange->checkedChanged().connect([=] {
            sinceDate->setDisabled(exportRange->checkedButton() != sinceBtn);
        });

        Wt::WText* sinceCount{ modal->bindNew<Wt::WText>("since-count", getListenCountText(toUTCStartOfDay(today, locale)), Wt::TextFormat::Plain) };
        // input, not change: some browsers fire change only once the field loses focus
        sinceDate->textInput().connect([=] {
            const Wt::WDate date{ sinceDate->date() };
            sinceCount->setText(date.isValid() ? getListenCountText(toUTCStartOfDay(date, locale)) : Wt::WString{});
        });

        modal->bindNew<Wt::WPushButton>("save-btn", Wt::WString::tr("Lms.save"))->clicked().connect([=, onSave = std::move(onSave)] {
            Wt::WDateTime exportSince;
            if (exportRange->checkedButton() == sinceBtn)
            {
                if (sinceDate->validate() != Wt::ValidationState::Valid)
                    return;

                exportSince = toUTCStartOfDay(sinceDate->date(), locale);
            }

            LmsApp->getModalManager().dispose(modalPtr);
            onSave(exportSince);
            LmsApp->notifyMsg(notification::Type::Info, Wt::WString::tr("Lms.Settings.settings-saved"));
        });

        modal->bindNew<Wt::WPushButton>("cancel-btn", Wt::WString::tr("Lms.cancel"))->clicked().connect([=] {
            LmsApp->getModalManager().dispose(modalPtr);
        });

        LmsApp->getModalManager().show(std::move(modal));
    }

    bool hasPendingExports(db::ScrobblingBackend backend)
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        db::ListenBackendSync::FindParameters params;
        params.setUser(LmsApp->getUserId()).setBackend(backend).setSyncState(db::SyncState::PendingAdd).setRange(db::Range{ 0, 1 });
        return !db::ListenBackendSync::findIds(LmsApp->getDbSession(), params).empty();
    }

    bool hasPendingExports(db::FeedbackBackend backend)
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        db::TrackFeedbackBackendSync::FindParameters params;
        params.setUser(LmsApp->getUserId()).setBackend(backend).setSyncState(db::SyncState::PendingAdd).setRange(db::Range{ 0, 1 });
        return !db::TrackFeedbackBackendSync::findIds(LmsApp->getDbSession(), params).empty();
    }
} // namespace lms::ui::utils
