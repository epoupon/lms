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

#include <Wt/WDate.h>

#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "database/MediaLibrary.hpp"
#include "database/ScanSettings.hpp"
#include "database/Session.hpp"

#include "scanners/ArtistInfoFileScanner.hpp"
#include "scanners/AudioFileScanner.hpp"
#include "scanners/ImageFileScanner.hpp"
#include "scanners/LyricsFileScanner.hpp"
#include "scanners/PlayListFileScanner.hpp"

#include "steps/ScanStepArtistReconciliation.hpp"
#include "steps/ScanStepAssociateArtistImages.hpp"
#include "steps/ScanStepAssociateExternalLyrics.hpp"
#include "steps/ScanStepAssociatePlayListTracks.hpp"
#include "steps/ScanStepAssociateReleaseImages.hpp"
#include "steps/ScanStepCheckForDuplicatedFiles.hpp"
#include "steps/ScanStepCheckForRemovedFiles.hpp"
#include "steps/ScanStepCompact.hpp"
#include "steps/ScanStepComputeClusterStats.hpp"
#include "steps/ScanStepDiscoverFiles.hpp"
#include "steps/ScanStepOptimize.hpp"
#include "steps/ScanStepRemoveOrphanedDbEntries.hpp"
#include "steps/ScanStepScanFiles.hpp"
#include "steps/ScanStepUpdateLibraryFields.hpp"

namespace lms::scanner
{
    using namespace db;

    namespace
    {
        static constexpr std::string_view currentSettingsName{ "" };
        static constexpr std::string_view lastScanSettingsName{ "last_scan" };

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

        std::optional<ScannerSettings> readScannerSettings(db::Session& session, std::string_view name)
        {
            std::optional<ScannerSettings> settings;

            auto transaction{ session.createReadTransaction() };

            const ScanSettings::pointer scanSettings{ ScanSettings::find(session, name) };
            if (!scanSettings)
                return settings;

            settings.emplace();
            settings->audioScanVersion = scanSettings->getAudioScanVersion();
            settings->artistInfoScanVersion = scanSettings->getArtistInfoScanVersion();
            settings->startTime = scanSettings->getUpdateStartTime();
            settings->updatePeriod = scanSettings->getUpdatePeriod();

            MediaLibrary::find(session, [&](const MediaLibrary::pointer& mediaLibrary) {
                MediaLibraryInfo info;
                info.firstScan = mediaLibrary->isEmpty();
                info.id = mediaLibrary->getId();
                info.rootDirectory = mediaLibrary->getPath().lexically_normal();

                settings->mediaLibraries.push_back(info);
            });

            {
                const auto& tags{ scanSettings->getExtraTagsToScan() };
                std::transform(std::cbegin(tags), std::cend(tags), std::back_inserter(settings->extraTags), [](std::string_view tag) { return std::string{ tag }; });
            }

            settings->artistTagDelimiters = scanSettings->getArtistTagDelimiters();
            settings->defaultTagDelimiters = scanSettings->getDefaultTagDelimiters();
            settings->artistsToNotSplit = scanSettings->getArtistsToNotSplit();

            settings->skipSingleReleasePlayLists = scanSettings->getSkipSingleReleasePlayLists();
            settings->allowArtistMBIDFallback = scanSettings->getAllowMBIDArtistMerge();

            // TODO, store this in DB + expose in UI
            settings->skipDuplicateTrackMBID = core::Service<core::IConfig>::get()->getBool("scanner-skip-duplicate-mbid", false);

            return settings;
        }

        void writeScannerSettings(db::Session& session, std::string_view name, const ScannerSettings& settings)
        {
            auto transaction{ session.createWriteTransaction() };

            ScanSettings::pointer scanSettings{ ScanSettings::find(session, name) };
            if (!scanSettings)
                scanSettings = session.create<ScanSettings>(name);

            scanSettings.modify()->setAllowMBIDArtistMerge(settings.allowArtistMBIDFallback);
            scanSettings.modify()->setSkipSingleReleasePlayLists(settings.skipSingleReleasePlayLists);
            // TODO add more fields
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
            _scheduleTimer.expires_after(std::chrono::seconds{ 0 });
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

        ScanContext scanContext;
        scanContext.scanOptions = scanOptions;
        ScanStats& stats{ scanContext.stats };
        stats.startTime = Wt::WDateTime::currentDateTime();

        processScanSteps(scanContext);

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

            // save current settings as last scan settings to compare during next scans if something changed
            writeScannerSettings(_db.getTLSSession(), lastScanSettingsName, _settings);
            _lastScanSettings = _settings;

            LMS_LOG(DBUPDATER, DEBUG, "Scan not aborted, scheduling next scan!");
            scheduleNextScan();

            _events.scanComplete.emit(stats);
        }
        else
        {
            LMS_LOG(DBUPDATER, DEBUG, "Scan aborted, not scheduling next scan!");
        }
    }

    void ScannerService::processScanSteps(ScanContext& context)
    {
        std::size_t stepIndex{};
        for (auto& scanStep : _scanSteps)
        {
            context.currentStepStats = ScanStepStats{
                .startTime = Wt::WDateTime::currentDateTime(),
                .stepCount = _scanSteps.size(),
                .stepIndex = stepIndex++,
                .currentStep = scanStep->getStep(),
                .totalElems = 0,
                .processedElems = 0
            };

            if (_abortScan)
                break;

            if (!scanStep->needProcess(context))
            {
                LMS_LOG(DBUPDATER, DEBUG, "Skipping scan step '" << scanStep->getStepName() << "'");
                continue;
            }

            {
                LMS_SCOPED_TRACE_OVERVIEW("Scanner", scanStep->getStepName());

                LMS_LOG(DBUPDATER, DEBUG, "Starting scan step '" << scanStep->getStepName() << "'");
                notifyInProgress(context.currentStepStats);
                scanStep->process(context);
                notifyInProgress(context.currentStepStats);
                LMS_LOG(DBUPDATER, DEBUG, "Completed scan step '" << scanStep->getStepName() << "'");
            }
        }
    }

    void ScannerService::refreshScanSettings()
    {
        std::optional<ScannerSettings> newSettings{ readScannerSettings(_db.getTLSSession(), currentSettingsName) };
        assert(newSettings.has_value());
        if (_settings == *newSettings)
            return;

        LMS_LOG(DBUPDATER, DEBUG, "Scanner settings updated");
        LMS_LOG(DBUPDATER, DEBUG, "Using audio scan version " << newSettings->audioScanVersion);
        LMS_LOG(DBUPDATER, DEBUG, "Using artist info scan version " << newSettings->artistInfoScanVersion);

        _settings = std::move(*newSettings);
        if (!_lastScanSettings)
            _lastScanSettings = readScannerSettings(_db.getTLSSession(), lastScanSettingsName);

        auto cbFunc{ [this](const ScanStepStats& stats) {
            notifyInProgressIfNeeded(stats);
        } };

        _fileScanners.clear();
        _fileScanners.emplace_back(std::make_unique<ArtistInfoFileScanner>(_settings, _db));
        _fileScanners.emplace_back(std::make_unique<AudioFileScanner>(_db, _settings));
        _fileScanners.emplace_back(std::make_unique<ImageFileScanner>(_db));
        _fileScanners.emplace_back(std::make_unique<LyricsFileScanner>(_db));
        _fileScanners.emplace_back(std::make_unique<PlayListFileScanner>(_db));

        std::vector<IFileScanner*> fileScanners;
        std::transform(std::cbegin(_fileScanners), std::cend(_fileScanners), std::back_inserter(fileScanners), [](const std::unique_ptr<IFileScanner>& scanner) { return scanner.get(); });

        ScanStepBase::InitParams params{
            .settings = _settings,
            .lastScanSettings = _lastScanSettings.has_value() ? &(_lastScanSettings.value()) : nullptr,
            .progressCallback = cbFunc,
            .abortScan = _abortScan,
            .db = _db,
            .fileScanners = fileScanners,
        };

        // Order is important: steps are sequential
        _scanSteps.clear();
        _scanSteps.emplace_back(std::make_unique<ScanStepDiscoverFiles>(params));
        _scanSteps.emplace_back(std::make_unique<ScanStepScanFiles>(params));
        _scanSteps.emplace_back(std::make_unique<ScanStepCheckForRemovedFiles>(params));
        _scanSteps.emplace_back(std::make_unique<ScanStepArtistReconciliation>(params));
        _scanSteps.emplace_back(std::make_unique<ScanStepAssociatePlayListTracks>(params));
        _scanSteps.emplace_back(std::make_unique<ScanStepUpdateLibraryFields>(params));
        _scanSteps.emplace_back(std::make_unique<ScanStepAssociateArtistImages>(params));
        _scanSteps.emplace_back(std::make_unique<ScanStepAssociateReleaseImages>(params));
        _scanSteps.emplace_back(std::make_unique<ScanStepAssociateExternalLyrics>(params));
        _scanSteps.emplace_back(std::make_unique<ScanStepRemoveOrphanedDbEntries>(params));
        _scanSteps.emplace_back(std::make_unique<ScanStepCompact>(params));
        _scanSteps.emplace_back(std::make_unique<ScanStepOptimize>(params));
        _scanSteps.emplace_back(std::make_unique<ScanStepComputeClusterStats>(params));
        _scanSteps.emplace_back(std::make_unique<ScanStepCheckForDuplicatedFiles>(params));
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
