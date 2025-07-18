/*
 * Copyright (C) 2023 Emeric Poupon
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

#include "IScanStep.hpp"
#include "ScanErrorLogger.hpp"

namespace lms::core
{
    class IJobScheduler;
}

namespace lms::db
{
    class IDb;
}

namespace lms::scanner
{
    class FileScanners;
    struct ScannerSettings;
    struct ScanStepStats;
    struct ScanContext;

    class ScanStepBase : public IScanStep
    {
    public:
        using ProgressCallback = std::function<void(const ScanStepStats& stats)>;

        struct InitParams
        {
            core::IJobScheduler& jobScheduler;
            const ScannerSettings& settings;
            const ScannerSettings* lastScanSettings{};
            ProgressCallback progressCallback;
            bool& abortScan;
            db::IDb& db;
            const FileScanners& fileScanners;
        };
        ScanStepBase(InitParams& initParams);
        ~ScanStepBase() override;
        ScanStepBase(const ScanStepBase&) = delete;
        ScanStepBase& operator=(const ScanStepBase&) = delete;

    protected:
        core::IJobScheduler& getJobScheduler() { return _jobScheduler; };
        const ScannerSettings* getLastScanSettings() const { return _lastScanSettings; }
        const FileScanners& getFileScanners() const { return _fileScanners; }

        void addError(ScanContext& context, std::shared_ptr<ScanError> error);

        template<typename T, typename... CtrArgs>
        void addError(ScanContext& context, CtrArgs&&... args)
        {
            auto error{ std::make_shared<T>(std::forward<CtrArgs>(args)...) };
            addError(context, error);
        }

        const ScannerSettings& _settings;
        ProgressCallback _progressCallback;
        bool& _abortScan;
        db::IDb& _db;

    private:
        core::IJobScheduler& _jobScheduler;
        const FileScanners& _fileScanners;

        const ScannerSettings* _lastScanSettings{};
        ScanErrorLogger _scanErrorLogger;
    };
} // namespace lms::scanner
