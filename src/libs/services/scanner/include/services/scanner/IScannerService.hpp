/*
 * Copyright (C) 2013 Emeric Poupon
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

#include <optional>

#include "ScannerEvents.hpp"
#include "ScannerOptions.hpp"
#include "ScannerStats.hpp"

namespace lms::db
{
    class IDb;
}

namespace lms::scanner
{
    class IScannerService
    {
    public:
        virtual ~IScannerService() = default;

        // Async requests
        virtual void requestReload() = 0; // will stop/reschedule scan
        virtual void requestImmediateScan(const ScanOptions& options = {}) = 0;

        enum class State
        {
            NotScheduled,
            Scheduled,
            InProgress,
        };

        struct Status
        {
            State currentState{ State::NotScheduled };
            Wt::WDateTime nextScheduledScan;
            std::optional<ScanStats> lastCompleteScanStats;
            std::optional<ScanStepStats> currentScanStepStats;
        };

        virtual Status getStatus() const = 0;

        virtual Events& getEvents() = 0;
    };

    std::unique_ptr<IScannerService> createScannerService(db::IDb& db);
} // namespace lms::scanner
