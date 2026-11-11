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

#include <Wt/WPushButton.h>
#include <Wt/WString.h>

#include "LmsApplication.hpp"

namespace lms::ui::utils
{
    void bindSaveDiscardButtons(Wt::WTemplateFormView* t, Wt::WFormModel* model, const std::function<void()>& saveData, const std::function<void()>& loadData)
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
                saveData();
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
} // namespace lms::ui::utils
