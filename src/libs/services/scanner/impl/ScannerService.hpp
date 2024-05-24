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

#include <chrono>
#include <optional>
#include <shared_mutex>
#include <vector>

#include <Wt/WDateTime.h>
#include <Wt/WIOService.h>
#include <Wt/WSignal.h>
#include <boost/asio/system_timer.hpp>

#include "IScanStep.hpp"
#include "ScannerSettings.hpp"
#include "core/Path.hpp"
#include "database/Db.hpp"
#include "database/Session.hpp"
#include "database/Types.hpp"
#include "services/scanner/IScannerService.hpp"

namespace lms::scanner
{
    class ScannerService : public IScannerService
    {
    public:
        ScannerService(db::Db& db);
        ~ScannerService();

    private:
        ScannerService(const ScannerService&) = delete;
        ScannerService& operator=(const ScannerService&) = delete;

        void requestReload() override;
        void requestImmediateScan(const ScanOptions& scanOptions) override;

        Status getStatus() const override;
        Events& getEvents() override { return _events; }

    private:
        void start();
        void stop();

        // Job handling
        void scheduleNextScan();
        void scheduleScan(const ScanOptions& scanOptions, const Wt::WDateTime& dateTime = {});

        void abortScan();

        // Update database (scheduled callback)
        void scan(const ScanOptions& scanOptions);

        void scanMediaDirectory(const std::filesystem::path& mediaDirectory, bool forceScan, ScanStats& stats);

        // Helpers
        void refreshScanSettings();
        ScannerSettings readSettings();

        void notifyInProgressIfNeeded(const ScanStepStats& stats);
        void notifyInProgress(const ScanStepStats& stats);

        std::vector<std::unique_ptr<IScanStep>> _scanSteps;

        std::mutex _controlMutex;
        bool _abortScan{};
        Wt::WIOService _ioService;
        boost::asio::system_timer _scheduleTimer{ _ioService };
        Events _events;
        std::chrono::system_clock::time_point _lastScanInProgressEmit{};
        db::Db& _db;

        mutable std::shared_mutex _statusMutex;
        State _curState{ State::NotScheduled };
        std::optional<ScanStats> _lastCompleteScanStats;
        std::optional<ScanStepStats> _currentScanStepStats;
        Wt::WDateTime _nextScheduledScan;

        ScannerSettings _settings;
    };
} // namespace lms::scanner
