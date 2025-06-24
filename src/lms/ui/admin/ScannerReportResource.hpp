/*
 * Copyright (C) 2025 Emeric Poupon
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

#include <Wt/WResource.h>

#include "services/scanner/ScannerStats.hpp"

namespace lms::ui
{

    class ScannerReportResource : public Wt::WResource
    {
    public:
        ScannerReportResource();
        ~ScannerReportResource() override;
        ScannerReportResource(const ScannerReportResource&) = delete;
        ScannerReportResource& operator=(const ScannerReportResource&) = delete;

        void setScanStats(const scanner::ScanStats& stats);
        void handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response) override;

    private:
        static Wt::WString formatScanError(const scanner::ScanError* error);
        static Wt::WString duplicateReasonToWString(scanner::DuplicateReason reason);
        std::unique_ptr<scanner::ScanStats> _stats;
    };

} // namespace lms::ui
