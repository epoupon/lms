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

#include "ScannerService.hpp"

#include <ctime>

#include "core/Exception.hpp"
#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "core/Path.hpp"
#include "database/MediaLibrary.hpp"
#include "database/ScanSettings.hpp"
#include "database/TrackFeatures.hpp"
#include "image/Image.hpp"

#include "ScanStepAssociateArtistImages.hpp"
#include "ScanStepAssociateExternalLyrics.hpp"
#include "ScanStepAssociateReleaseImages.hpp"
#include "ScanStepCheckForDuplicatedFiles.hpp"
#include "ScanStepCheckForRemovedFiles.hpp"
#include "ScanStepCompact.hpp"
#include "ScanStepComputeClusterStats.hpp"
#include "ScanStepDiscoverFiles.hpp"
#include "ScanStepOptimize.hpp"
#include "ScanStepRemoveOrphanedDbEntries.hpp"
#include "ScanStepScanFiles.hpp"
#include "ScanStepUpdateLibraryFields.hpp"

namespace lms::scanner
{
    using namespace db;

    namespace
    {
        Wt::WDate getNextMonday(Wt::WDate current)
        {
            do
            {
                current = current.addDays(1);
            } while (current.dayOfWeek() != 1);

            return current;
        }

        Wt::WDate getNextFirstOfMonth(Wt::WDate current)
        {
            do
            {
                current = current.addDays(1);
            } while (current.day() != 1);

            return current;
        }
    } // namespace

    std::unique_ptr<IScannerService> createScannerService(Db& db)
    {
        return std::make_unique<ScannerService>(db);
    }

    ScannerService::ScannerService(Db& db)
        : _db{ db }
    {
        _ioService.setThreadCount(1);

        refreshScanSettings();

        start();
    }

    ScannerService::~ScannerService()
    {
        LMS_LOG(DBUPDATER, INFO, "Stopping service...");
        stop();
        LMS_LOG(DBUPDATER, INFO, "Service stopped!");
    }

    void ScannerService::start()
    {
        std::scoped_lock lock{ _controlMutex };

        _ioService.post([this] {
            if (_abortScan)
                return;

            scheduleNextScan();
        });

        _ioService.start();
    }

    void ScannerService::stop()
    {
        std::scoped_lock lock{ _controlMutex };

        _abortScan = true;
        _scheduleTimer.cancel();
        _ioService.stop();
    }

    void ScannerService::abortScan()
    {
        bool isRunning{};
        {
            std::scoped_lock lock{ _statusMutex };
            isRunning = _curState == State::InProgress;
        }

        LMS_LOG(DBUPDATER, DEBUG, "Aborting scan...");
        std::scoped_lock lock{ _controlMutex };

        LMS_LOG(DBUPDATER, DEBUG, "Waiting for the scan to abort...");

        _abortScan = true;
        _scheduleTimer.cancel();
        _ioService.stop();
        LMS_LOG(DBUPDATER, DEBUG, "Scan abort done!");

        _abortScan = false;
        _ioService.start();

        if (isRunning)
            _events.scanAborted.emit();
    }

    void ScannerService::requestImmediateScan(const ScanOptions& scanOptions)
    {
        abortScan();
        _ioService.post([this, scanOptions] {
            if (_abortScan)
                return;

            scheduleScan(scanOptions);
        });
    }

    void ScannerService::requestReload()
    {
        abortScan();
        _ioService.post([this]() {
            if (_abortScan)
                return;

            scheduleNextScan();
        });
    }

    ScannerService::Status ScannerService::getStatus() const
    {
        Status res;

        std::shared_lock lock{ _statusMutex };

        res.currentState = _curState;
        res.nextScheduledScan = _nextScheduledScan;
        res.lastCompleteScanStats = _lastCompleteScanStats;
        res.currentScanStepStats = _currentScanStepStats;

        return res;
    }

    void ScannerService::scheduleNextScan()
    {
        LMS_LOG(DBUPDATER, DEBUG, "Scheduling next scan");

        refreshScanSettings();

        const Wt::WDateTime now{ Wt::WDateTime::currentDateTime() };

        Wt::WDateTime nextScanDateTime;
        switch (_settings.updatePeriod)
        {
        case ScanSettings::UpdatePeriod::Daily:
            if (now.time() < _settings.startTime)
                nextScanDateTime = { now.date(), _settings.startTime };
            else
                nextScanDateTime = { now.date().addDays(1), _settings.startTime };
            break;

        case ScanSettings::UpdatePeriod::Weekly:
            if (now.time() < _settings.startTime && now.date().dayOfWeek() == 1)
                nextScanDateTime = { now.date(), _settings.startTime };
            else
                nextScanDateTime = { getNextMonday(now.date()), _settings.startTime };
            break;

        case ScanSettings::UpdatePeriod::Monthly:
            if (now.time() < _settings.startTime && now.date().day() == 1)
                nextScanDateTime = { now.date(), _settings.startTime };
            else
                nextScanDateTime = { getNextFirstOfMonth(now.date()), _settings.startTime };
            break;

        case ScanSettings::UpdatePeriod::Hourly:
            nextScanDateTime = { now.date(), now.time().addSecs(3600) };
            break;

        case ScanSettings::UpdatePeriod::Never:
            LMS_LOG(DBUPDATER, INFO, "Auto scan disabled!");
            break;
        }

        if (nextScanDateTime.isValid())
            scheduleScan(ScanOptions{}, nextScanDateTime);

        {
            std::unique_lock lock{ _statusMutex };
            _curState = nextScanDateTime.isValid() ? State::Scheduled : State::NotScheduled;
            _nextScheduledScan = nextScanDateTime;
        }

        _events.scanScheduled.emit(_nextScheduledScan);
    }

    void ScannerService::scheduleScan(const ScanOptions& scanOptions, const Wt::WDateTime& dateTime)
    {
        auto cb{ [this, scanOptions](boost::system::error_code ec) {
            if (ec)
                return;

            scan(scanOptions);
        } };

        if (dateTime.isNull())
        {
            LMS_LOG(DBUPDATER, INFO, "Scheduling next scan right now");
            _scheduleTimer.expires_from_now(std::chrono::seconds{ 0 });
            _scheduleTimer.async_wait(cb);
        }
        else
        {
            std::chrono::system_clock::time_point timePoint{ dateTime.toTimePoint() };
            std::time_t t{ std::chrono::system_clock::to_time_t(timePoint) };
            char ctimeStr[26];

            LMS_LOG(DBUPDATER, INFO, "Scheduling next scan at " << std::string(::ctime_r(&t, ctimeStr)));
            _scheduleTimer.expires_at(timePoint);
            _scheduleTimer.async_wait(cb);
        }
    }

    void ScannerService::scan(const ScanOptions& scanOptions)
    {
        LMS_SCOPED_TRACE_OVERVIEW("Scanner", "Scan");

        _events.scanStarted.emit();

        {
            std::unique_lock lock{ _statusMutex };
            _nextScheduledScan = {};
        }

        LMS_LOG(UI, INFO, "New scan started!");

        refreshScanSettings();

        IScanStep::ScanContext scanContext;
        scanContext.scanOptions = scanOptions;
        ScanStats& stats{ scanContext.stats };
        stats.startTime = Wt::WDateTime::currentDateTime();

        std::size_t stepIndex{};
        for (auto& scanStep : _scanSteps)
        {
            LMS_SCOPED_TRACE_OVERVIEW("Scanner", scanStep->getStepName());

            LMS_LOG(DBUPDATER, DEBUG, "Starting scan step '" << scanStep->getStepName() << "'");
            scanContext.currentStepStats = ScanStepStats{
                .startTime = Wt::WDateTime::currentDateTime(),
                .stepCount = _scanSteps.size(),
                .stepIndex = stepIndex++,
                .currentStep = scanStep->getStep(),
                .totalElems = 0,
                .processedElems = 0
            };

            notifyInProgress(scanContext.currentStepStats);
            scanStep->process(scanContext);
            notifyInProgress(scanContext.currentStepStats);
            LMS_LOG(DBUPDATER, DEBUG, "Completed scan step '" << scanStep->getStepName() << "'");
        }

        {
            std::unique_lock lock{ _statusMutex };

            _curState = State::NotScheduled;
            _currentScanStepStats.reset(); // must be sync with _curState
        }

        LMS_LOG(DBUPDATER, INFO, "Scan " << (_abortScan ? "aborted" : "complete") << ". Changes = " << stats.nbChanges() << " (added = " << stats.additions << ", removed = " << stats.deletions << ", updated = " << stats.updates << "), Not changed = " << stats.skips << ", Scanned = " << stats.scans << " (errors = " << stats.errors.size() << "), features fetched = " << stats.featuresFetched << ",  duplicates = " << stats.duplicates.size());

        if (!_abortScan)
        {
            stats.stopTime = Wt::WDateTime::currentDateTime();

            {
                std::unique_lock lock{ _statusMutex };
                _lastCompleteScanStats = stats;
            }

            LMS_LOG(DBUPDATER, DEBUG, "Scan not aborted, scheduling next scan!");
            scheduleNextScan();

            _events.scanComplete.emit(stats);
        }
        else
        {
            LMS_LOG(DBUPDATER, DEBUG, "Scan aborted, not scheduling next scan!");
        }
    }

    void ScannerService::refreshScanSettings()
    {
        ScannerSettings newSettings{ readSettings() };
        if (_settings == newSettings)
            return;

        LMS_LOG(DBUPDATER, DEBUG, "Scanner settings updated");
        LMS_LOG(DBUPDATER, DEBUG, "skipDuplicateMBID = " << newSettings.skipDuplicateMBID);
        LMS_LOG(DBUPDATER, DEBUG, "Using scan settings version " << newSettings.scanVersion);

        _settings = std::move(newSettings);

        auto cbFunc{ [this](const ScanStepStats& stats) {
            notifyInProgressIfNeeded(stats);
        } };

        ScanStepBase::InitParams params{
            _settings,
            cbFunc,
            _abortScan,
            _db
        };

        // Order is important: steps are sequential
        _scanSteps.clear();
        _scanSteps.push_back(std::make_unique<ScanStepDiscoverFiles>(params));
        _scanSteps.push_back(std::make_unique<ScanStepScanFiles>(params));
        _scanSteps.push_back(std::make_unique<ScanStepCheckForRemovedFiles>(params));
        _scanSteps.push_back(std::make_unique<ScanStepUpdateLibraryFields>(params));
        _scanSteps.push_back(std::make_unique<ScanStepAssociateArtistImages>(params));
        _scanSteps.push_back(std::make_unique<ScanStepAssociateReleaseImages>(params));
        _scanSteps.push_back(std::make_unique<ScanStepAssociateExternalLyrics>(params));
        _scanSteps.push_back(std::make_unique<ScanStepRemoveOrphanedDbEntries>(params));
        _scanSteps.push_back(std::make_unique<ScanStepCompact>(params));
        _scanSteps.push_back(std::make_unique<ScanStepOptimize>(params));
        _scanSteps.push_back(std::make_unique<ScanStepComputeClusterStats>(params));
        _scanSteps.push_back(std::make_unique<ScanStepCheckForDuplicatedFiles>(params));
    }

    ScannerSettings ScannerService::readSettings()
    {
        ScannerSettings newSettings;

        newSettings.skipDuplicateMBID = core::Service<core::IConfig>::get()->getBool("scanner-skip-duplicate-mbid", false);
        {
            auto transaction{ _db.getTLSSession().createReadTransaction() };

            const ScanSettings::pointer scanSettings{ ScanSettings::get(_db.getTLSSession()) };

            newSettings.scanVersion = scanSettings->getScanVersion();
            newSettings.startTime = scanSettings->getUpdateStartTime();
            newSettings.updatePeriod = scanSettings->getUpdatePeriod();

            {
                const auto audioFileExtensions{ scanSettings->getAudioFileExtensions() };
                newSettings.supportedAudioFileExtensions.reserve(audioFileExtensions.size());
                std::transform(std::cbegin(audioFileExtensions), std::cend(audioFileExtensions), std::back_inserter(newSettings.supportedAudioFileExtensions),
                    [](const std::filesystem::path& extension) { return std::filesystem::path{ core::stringUtils::stringToLower(extension.string()) }; });
            }

            {
                const auto imageFileExtensions{ image::getSupportedFileExtensions() };
                newSettings.supportedImageFileExtensions.reserve(imageFileExtensions.size());
                std::transform(std::cbegin(imageFileExtensions), std::cend(imageFileExtensions), std::back_inserter(newSettings.supportedImageFileExtensions),
                    [](const std::filesystem::path& extension) { return std::filesystem::path{ core::stringUtils::stringToLower(extension.string()) }; });
            }

            {
                const auto lyricsFileExtensions{ metadata::getSupportedLyricsFileExtensions() };
                newSettings.supportedLyricsFileExtensions.reserve(lyricsFileExtensions.size());
                std::transform(std::cbegin(lyricsFileExtensions), std::cend(lyricsFileExtensions), std::back_inserter(newSettings.supportedLyricsFileExtensions),
                    [](const std::filesystem::path& extension) { return std::filesystem::path{ core::stringUtils::stringToLower(extension.string()) }; });
            }

            MediaLibrary::find(_db.getTLSSession(), [&](const MediaLibrary::pointer& mediaLibrary) {
                newSettings.mediaLibraries.push_back(ScannerSettings::MediaLibraryInfo{ mediaLibrary->getId(), mediaLibrary->getPath().lexically_normal() });
            });

            {
                const auto& tags{ scanSettings->getExtraTagsToScan() };
                std::transform(std::cbegin(tags), std::cend(tags), std::back_inserter(newSettings.extraTags), [](std::string_view tag) { return std::string{ tag }; });
            }

            newSettings.artistTagDelimiters = scanSettings->getArtistTagDelimiters();
            newSettings.defaultTagDelimiters = scanSettings->getDefaultTagDelimiters();
        }

        return newSettings;
    }

    void ScannerService::notifyInProgress(const ScanStepStats& stepStats)
    {
        {
            std::unique_lock lock{ _statusMutex };

            _curState = State::InProgress;
            _currentScanStepStats = stepStats;
        }

        const std::chrono::system_clock::time_point now{ std::chrono::system_clock::now() };
        _events.scanInProgress.emit(stepStats);
        _lastScanInProgressEmit = now;
    }

    void ScannerService::notifyInProgressIfNeeded(const ScanStepStats& stepStats)
    {
        std::chrono::system_clock::time_point now{ std::chrono::system_clock::now() };

        if (now - _lastScanInProgressEmit >= std::chrono::seconds{ 1 })
            notifyInProgress(stepStats);
    }
} // namespace lms::scanner
