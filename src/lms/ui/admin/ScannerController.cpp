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

#include "ScannerController.hpp"

#include <iomanip>

#include <Wt/Http/Response.h>
#include <Wt/WCheckBox.h>
#include <Wt/WDateTime.h>
#include <Wt/WLocale.h>
#include <Wt/WPushButton.h>
#include <Wt/WResource.h>

#include "core/Service.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "services/scanner/IScannerService.hpp"

#include "LmsApplication.hpp"

namespace lms::ui
{
    namespace
    {
        std::string durationToString(const Wt::WDateTime& begin, const Wt::WDateTime& end)
        {
            return begin.timeTo(end).toUTF8();
        }
    } // namespace

    class ReportResource : public Wt::WResource
    {
    public:
        ReportResource()
        {
            suggestFileName("report.txt");
        }

        ~ReportResource()
        {
            beingDeleted();
        }

        void setScanStats(const scanner::ScanStats& stats)
        {
            if (!_stats)
                _stats = std::make_unique<scanner::ScanStats>();

            *_stats = stats;
        }

        void handleRequest(const Wt::Http::Request&, Wt::Http::Response& response)
        {
            if (!_stats)
                return;

            response.out() << Wt::WString::tr("Lms.Admin.ScannerController.errors-header").arg(_stats->errors.size()).toUTF8() << std::endl;

            for (const auto& error : _stats->errors)
            {
                response.out() << error.file.string() << " - " << errorTypeToWString(error.error).toUTF8();
                if (!error.systemError.empty())
                    response.out() << ": " << error.systemError;
                response.out() << std::endl;
            }

            response.out() << std::endl;

            response.out() << Wt::WString::tr("Lms.Admin.ScannerController.duplicates-header").arg(_stats->duplicates.size()).toUTF8() << std::endl;

            {
                auto transaction{ LmsApp->getDbSession().createReadTransaction() };

                for (const auto& duplicate : _stats->duplicates)
                {
                    const auto& track{ db::Track::find(LmsApp->getDbSession(), duplicate.trackId) };
                    if (!track)
                        continue;

                    response.out() << track->getAbsoluteFilePath().string();
                    if (auto mbid{ track->getTrackMBID() })
                        response.out() << " (Track MBID " << mbid->getAsString() << ")";

                    response.out() << " - " << duplicateReasonToWString(duplicate.reason).toUTF8() << '\n';
                }
            }
        }

    private:
        static Wt::WString errorTypeToWString(scanner::ScanErrorType error)
        {
            switch (error)
            {
            case scanner::ScanErrorType::CannotReadFile:
                return Wt::WString::tr("Lms.Admin.ScannerController.cannot-read-file");
            case scanner::ScanErrorType::CannotReadAudioFile:
                return Wt::WString::tr("Lms.Admin.ScannerController.cannot-read-audio-file");
            case scanner::ScanErrorType::CannotReadImageFile:
                return Wt::WString::tr("Lms.Admin.ScannerController.cannot-read-image-file");
            case scanner::ScanErrorType::CannotReadLyricsFile:
                return Wt::WString::tr("Lms.Admin.ScannerController.cannot-read-lyrics-file");
            case scanner::ScanErrorType::NoAudioTrack:
                return Wt::WString::tr("Lms.Admin.ScannerController.no-audio-track");
            case scanner::ScanErrorType::BadDuration:
                return Wt::WString::tr("Lms.Admin.ScannerController.bad-duration");
            }
            return "?";
        }

        static Wt::WString duplicateReasonToWString(scanner::DuplicateReason reason)
        {
            switch (reason)
            {
            case scanner::DuplicateReason::SameHash:
                return Wt::WString::tr("Lms.Admin.ScannerController.same-hash");
            case scanner::DuplicateReason::SameTrackMBID:
                return Wt::WString::tr("Lms.Admin.ScannerController.same-mbid");
            }
            return "?";
        }

        std::unique_ptr<scanner::ScanStats> _stats;
    };

    ScannerController::ScannerController()
        : WTemplate{ Wt::WString::tr("Lms.Admin.ScannerController.template") }
    {
        addFunction("tr", &Wt::WTemplate::Functions::tr);
        addFunction("id", &Wt::WTemplate::Functions::id);

        using namespace scanner;

        {
            _reportBtn = bindNew<Wt::WPushButton>("report-btn", Wt::WString::tr("Lms.Admin.ScannerController.get-report"));

            auto reportResource{ std::make_shared<ReportResource>() };
            reportResource->setTakesUpdateLock(true);
            _reportResource = reportResource.get();

            Wt::WLink link{ reportResource };
            link.setTarget(Wt::LinkTarget::NewWindow);
            _reportBtn->setLink(link);
        }

        Wt::WCheckBox* fullScan{ bindNew<Wt::WCheckBox>("full-scan") };
        Wt::WCheckBox* forceOptimize{ bindNew<Wt::WCheckBox>("force-optimize") };
        Wt::WCheckBox* compact{ bindNew<Wt::WCheckBox>("compact") };
        Wt::WPushButton* scanBtn{ bindNew<Wt::WPushButton>("scan-btn", Wt::WString::tr("Lms.Admin.ScannerController.scan-now")) };
        scanBtn->clicked().connect([=] {
            const scanner::ScanOptions scanOptions{
                .fullScan = fullScan->isChecked(),
                .forceOptimize = forceOptimize->isChecked(),
                .compact = compact->isChecked(),
            };
            core::Service<scanner::IScannerService>::get()->requestImmediateScan(scanOptions);
        });

        _lastScanStatus = bindNew<Wt::WLineEdit>("last-scan");
        _lastScanStatus->setReadOnly(true);

        _status = bindNew<Wt::WLineEdit>("status");
        _status->setReadOnly(true);

        _stepStatus = bindNew<Wt::WLineEdit>("step-status");
        _stepStatus->setReadOnly(true);

        auto onDbEvent{ [&]() { refreshContents(); } };

        LmsApp->getScannerEvents().scanAborted.connect(this, [] {
            LmsApp->notifyMsg(Notification::Type::Info, Wt::WString::tr("Lms.Admin.Database.database"), Wt::WString::tr("Lms.Admin.Database.scan-aborted"));
        });
        LmsApp->getScannerEvents().scanStarted.connect(this, [] {
            LmsApp->notifyMsg(Notification::Type::Info, Wt::WString::tr("Lms.Admin.Database.database"), Wt::WString::tr("Lms.Admin.Database.scan-launched"));
        });
        LmsApp->getScannerEvents().scanComplete.connect(this, onDbEvent);
        LmsApp->getScannerEvents().scanInProgress.connect(this, onDbEvent);
        LmsApp->getScannerEvents().scanScheduled.connect(this, onDbEvent);

        refreshContents();
    }

    void ScannerController::refreshContents()
    {
        using namespace scanner;

        const IScannerService::Status status{ core::Service<IScannerService>::get()->getStatus() };

        refreshLastScanStatus(status);
        refreshStatus(status);
    }

    void ScannerController::refreshLastScanStatus(const scanner::IScannerService::Status& status)
    {
        if (status.lastCompleteScanStats)
        {
            _lastScanStatus->setText(Wt::WString::tr("Lms.Admin.ScannerController.last-scan-status")
                                         .arg(status.lastCompleteScanStats->nbFiles())
                                         .arg(durationToString(status.lastCompleteScanStats->startTime, status.lastCompleteScanStats->stopTime))
                                         .arg(status.lastCompleteScanStats->stopTime.date().toString(Wt::WLocale::currentLocale().dateFormat()))
                                         .arg(status.lastCompleteScanStats->stopTime.time().toString(Wt::WLocale::currentLocale().timeFormat()))
                                         .arg(status.lastCompleteScanStats->errors.size())
                                         .arg(status.lastCompleteScanStats->duplicates.size()));

            _reportResource->setScanStats(*status.lastCompleteScanStats);
            _reportBtn->setEnabled(true);
        }
        else
        {
            _lastScanStatus->setText(Wt::WString::tr("Lms.Admin.ScannerController.last-scan-not-available"));
            _reportBtn->setEnabled(false);
        }
    }

    void ScannerController::refreshStatus(const scanner::IScannerService::Status& status)
    {
        using namespace scanner;

        switch (status.currentState)
        {
        case IScannerService::State::NotScheduled:
            _status->setText(Wt::WString::tr("Lms.Admin.ScannerController.status-not-scheduled"));
            _stepStatus->setText("");
            break;

        case IScannerService::State::Scheduled:
            _status->setText(Wt::WString::tr("Lms.Admin.ScannerController.status-scheduled")
                                 .arg(status.nextScheduledScan.date().toString(Wt::WLocale::currentLocale().dateFormat()))
                                 .arg(status.nextScheduledScan.time().toString(Wt::WLocale::currentLocale().timeFormat())));
            _stepStatus->setText("");
            break;

        case IScannerService::State::InProgress:
            assert(status.currentScanStepStats);

            _status->setText(Wt::WString::tr("Lms.Admin.ScannerController.status-in-progress")
                                 .arg(status.currentScanStepStats->stepIndex + 1)
                                 .arg(status.currentScanStepStats->stepCount));

            refreshCurrentStep(*status.currentScanStepStats);
            break;
        }
    }

    void ScannerController::refreshCurrentStep(const scanner::ScanStepStats& stepStats)
    {
        using namespace scanner;

        switch (stepStats.currentStep)
        {
        case ScanStep::AssociateArtistImages:
            _stepStatus->setText(Wt::WString::tr("Lms.Admin.ScannerController.step-associating-artist-images")
                                     .arg(stepStats.progress()));
            break;

        case ScanStep::AssociateExternalLyrics:
            _stepStatus->setText(Wt::WString::tr("Lms.Admin.ScannerController.step-associating-external-lyrics")
                                     .arg(stepStats.progress()));
            break;

        case ScanStep::AssociateReleaseImages:
            _stepStatus->setText(Wt::WString::tr("Lms.Admin.ScannerController.step-associating-release-images")
                                     .arg(stepStats.progress()));
            break;

        case ScanStep::CheckForDuplicatedFiles:
            _stepStatus->setText(Wt::WString::tr("Lms.Admin.ScannerController.step-checking-for-duplicate-files")
                                     .arg(stepStats.processedElems));
            break;

        case ScanStep::CheckForRemovedFiles:
            _stepStatus->setText(Wt::WString::tr("Lms.Admin.ScannerController.step-checking-for-removed-files")
                                     .arg(stepStats.progress()));
            break;

        case ScanStep::Compact:
            _stepStatus->setText(Wt::WString::tr("Lms.Admin.ScannerController.step-compact"));
            break;

        case ScanStep::ComputeClusterStats:
            _stepStatus->setText(Wt::WString::tr("Lms.Admin.ScannerController.step-compute-cluster-stats")
                                     .arg(stepStats.progress()));
            break;

        case ScanStep::DiscoverFiles:
            _stepStatus->setText(Wt::WString::tr("Lms.Admin.ScannerController.step-discovering-files")
                                     .arg(stepStats.processedElems));
            break;

        case ScanStep::FetchTrackFeatures:
            _stepStatus->setText(Wt::WString::tr("Lms.Admin.ScannerController.step-fetching-track-features")
                                     .arg(stepStats.processedElems)
                                     .arg(stepStats.totalElems)
                                     .arg(stepStats.progress()));
            break;

        case ScanStep::Optimize:
            _stepStatus->setText(Wt::WString::tr("Lms.Admin.ScannerController.step-optimize")
                                     .arg(stepStats.progress()));
            break;

        case ScanStep::RemoveOrphanedDbEntries:
            _stepStatus->setText(Wt::WString::tr("Lms.Admin.ScannerController.step-removing-orphaned-entries")
                                     .arg(stepStats.processedElems));
            break;

        case ScanStep::ReloadSimilarityEngine:
            _stepStatus->setText(Wt::WString::tr("Lms.Admin.ScannerController.step-reloading-similarity-engine")
                                     .arg(stepStats.progress()));
            break;

        case ScanStep::ScanFiles:
            _stepStatus->setText(Wt::WString::tr("Lms.Admin.ScannerController.step-scanning-files")
                                     .arg(stepStats.processedElems)
                                     .arg(stepStats.totalElems)
                                     .arg(stepStats.progress()));
            break;

        case ScanStep::UpdateLibraryFields:
            _stepStatus->setText(Wt::WString::tr("Lms.Admin.ScannerController.step-updating-library-fields")
                                     .arg(stepStats.processedElems));
            break;
        }
    }
} // namespace lms::ui
