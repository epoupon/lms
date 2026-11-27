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

#pragma once

#include <functional>

#include <Wt/WDateTime.h>
#include <Wt/WFormModel.h>
#include <Wt/WString.h>
#include <Wt/WTemplateFormView.h>

#include "database/objects/Types.hpp"

namespace lms::ui::utils
{
    // saveData returns false when the save waits for a confirmation: it then reports the save itself
    void bindSaveDiscardButtons(Wt::WTemplateFormView* t, Wt::WFormModel* model, const std::function<bool()>& saveData, const std::function<void()>& loadData);

    // Asks which existing listens to export when turning on a scrobbling backend, cancelling aborts the save
    void showExportListensModal(const Wt::WString& backendName, std::function<void(const Wt::WDateTime& exportSince)> onSave);

    // Whether some of the current user's listens/feedbacks are waiting to be sent to the backend
    bool hasPendingExports(db::ScrobblingBackend backend);
    bool hasPendingExports(db::FeedbackBackend backend);
} // namespace lms::ui::utils
