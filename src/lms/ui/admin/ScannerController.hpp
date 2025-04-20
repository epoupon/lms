/*
 * Copyright (C) 2019 Emeric Poupon
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

#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WTemplate.h>

#include "services/scanner/IScannerService.hpp"

namespace lms::ui
{
    class ScannerController : public Wt::WTemplate
    {
    public:
        ScannerController();

    private:
        void refreshContents();
        void refreshLastScanStatus(const scanner::IScannerService::Status& status);
        void refreshStatus(const scanner::IScannerService::Status& status);
        void refreshCurrentStep(const scanner::ScanStepStats& stepStats);

        Wt::WPushButton* _reportBtn;
        Wt::WLineEdit* _lastScanStatus;
        Wt::WLineEdit* _status;
        Wt::WLineEdit* _stepStatus;
        class ScannerReportResource* _reportResource;
    };
} // namespace lms::ui
