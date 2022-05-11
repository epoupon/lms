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
#include <Wt/WDateTime.h>
#include <Wt/WPushButton.h>
#include <Wt/WResource.h>

#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "services/scanner/IScannerService.hpp"
#include "utils/Service.hpp"
#include "LmsApplication.hpp"

namespace UserInterface {

static
std::string
durationToString(const Wt::WDateTime& begin, const Wt::WDateTime& end)
{
	const auto secs {std::chrono::duration_cast<std::chrono::seconds>(end.toTimePoint() - begin.toTimePoint()).count()};

	std::ostringstream oss;

	if (secs >= 3600)
		oss << secs/3600 << "h";
	if (secs >= 60)
		oss << std::setw(2) << std::setfill('0') << (secs % 3600) / 60 << "m";
	oss << std::setw(2) << std::setfill('0') << (secs % 60) << "s";

	return oss.str();
}


class ReportResource : public Wt::WResource
{
	public:

		ReportResource(const Scanner::ScanStats& stats)
		: _stats {stats}
		{
			suggestFileName("report.txt");
		}

		~ReportResource()
		{
			beingDeleted();
		}

		void handleRequest(const Wt::Http::Request&, Wt::Http::Response& response)
		{
			response.out() << Wt::WString::tr("Lms.Admin.ScannerController.errors-header").arg(_stats.errors.size()).toUTF8() << std::endl;

			for (const auto& error : _stats.errors)
			{
				response.out() << error.file.string() << " - " << errorTypeToWString(error.error).toUTF8();
				if (!error.systemError.empty())
					response.out() << ": " << error.systemError;
				response.out() << std::endl;
			}

			response.out() << std::endl;

			response.out() << Wt::WString::tr("Lms.Admin.ScannerController.duplicates-header").arg(_stats.duplicates.size()).toUTF8() << std::endl;

			{
				auto transaction {LmsApp->getDbSession().createSharedTransaction()};

				for (const auto& duplicate : _stats.duplicates)
				{
					const auto& track {Database::Track::find(LmsApp->getDbSession(), duplicate.trackId)};
					if (!track)
						continue;

					response.out() << track->getPath().string();
					if (auto mbid {track->getTrackMBID()})
						response.out() << " (Recording MBID " << mbid->getAsString() << ")";

					response.out() << " - " << duplicateReasonToWString(duplicate.reason).toUTF8() << '\n';
				}
			}
		}

	private:

		static Wt::WString errorTypeToWString(Scanner::ScanErrorType error)
		{
			switch (error)
			{
				case Scanner::ScanErrorType::CannotReadFile: return Wt::WString::tr("Lms.Admin.ScannerController.cannot-read-file");
				case Scanner::ScanErrorType::CannotParseFile: return Wt::WString::tr("Lms.Admin.ScannerController.cannot-parse-file");
				case Scanner::ScanErrorType::NoAudioTrack: return Wt::WString::tr("Lms.Admin.ScannerController.no-audio-track");
				case Scanner::ScanErrorType::BadDuration: return Wt::WString::tr("Lms.Admin.ScannerController.bad-duration");
			}
			return "?";
		}

		static Wt::WString duplicateReasonToWString(Scanner::DuplicateReason reason)
		{
			switch (reason)
			{
				case Scanner::DuplicateReason::SameHash: return Wt::WString::tr("Lms.Admin.ScannerController.same-hash");
				case Scanner::DuplicateReason::SameRecordingMBID: return Wt::WString::tr("Lms.Admin.ScannerController.same-mbid");
			}
			return "?";
		}

		Scanner::ScanStats _stats;
};


ScannerController::ScannerController()
: WTemplate {Wt::WString::tr("Lms.Admin.ScannerController.template")}
{
	addFunction("tr", &Wt::WTemplate::Functions::tr);

	using namespace Scanner;

	auto onDbEvent = [&]() { refreshContents(); };

	LmsApp->getScannerEvents().scanStarted.connect(this, []
	{
		LmsApp->notifyMsg(Notification::Type::Info, Wt::WString::tr("Lms.Admin.Database.database"), Wt::WString::tr("Lms.Admin.Database.scan-launched"));
	});
	LmsApp->getScannerEvents().scanComplete.connect(this, onDbEvent);
	LmsApp->getScannerEvents().scanInProgress.connect(this, onDbEvent);
	LmsApp->getScannerEvents().scanScheduled.connect(this, onDbEvent);

	refreshContents();
}

void
ScannerController::refreshContents()
{
	using namespace Scanner;

	Wt::WPushButton* reportBtn {bindNew<Wt::WPushButton>("report-btn", Wt::WString::tr("Lms.Admin.ScannerController.get-report"))};

	Wt::WPushButton* scanBtn {bindNew<Wt::WPushButton>("scan-btn", Wt::WString::tr("Lms.Admin.ScannerController.scan-now"))};
	scanBtn->clicked().connect([]
	{
		Service<Scanner::IScannerService>::get()->requestImmediateScan(false);
	});

	Wt::WPushButton* fullScanBtn {bindNew<Wt::WPushButton>("full-scan-btn", Wt::WString::tr("Lms.Admin.ScannerController.force-scan-now"))};
	fullScanBtn->clicked().connect([]
	{
		Service<Scanner::IScannerService>::get()->requestImmediateScan(true);
	});

	const IScannerService::Status status {Service<IScannerService>::get()->getStatus()};
	if (status.lastCompleteScanStats)
	{
		bindString("last-scan", Wt::WString::tr("Lms.Admin.ScannerController.last-scan-status")
				.arg(status.lastCompleteScanStats->nbFiles())
				.arg(durationToString(status.lastCompleteScanStats->startTime, status.lastCompleteScanStats->stopTime))
				.arg(status.lastCompleteScanStats->stopTime.toString())
				.arg(status.lastCompleteScanStats->errors.size())
				.arg(status.lastCompleteScanStats->duplicates.size())
			  );

		Wt::WLink link {std::make_shared<ReportResource>(*status.lastCompleteScanStats)};
		link.setTarget(Wt::LinkTarget::NewWindow);
		reportBtn->setLink(link);
	}
	else
	{
		bindString("last-scan", Wt::WString::tr("Lms.Admin.ScannerController.last-scan-not-available"));
		reportBtn->setEnabled(false);
	}


	switch (status.currentState)
	{
		case IScannerService::State::NotScheduled:
			bindString("status", Wt::WString::tr("Lms.Admin.ScannerController.status-not-scheduled"));
			bindEmpty("step-status");
			break;
		case IScannerService::State::Scheduled:
			bindString("status", Wt::WString::tr("Lms.Admin.ScannerController.status-scheduled")
					.arg(status.nextScheduledScan.toString()));
			bindEmpty("step-status");
			break;
		case IScannerService::State::InProgress:
			bindString("status", Wt::WString::tr("Lms.Admin.ScannerController.status-in-progress")
					.arg(static_cast<int>(status.currentScanStepStats->currentStep) + 1)
					.arg(Scanner::ScanProgressStepCount));

			switch (status.currentScanStepStats->currentStep)
			{
				case Scanner::ScanProgressStep::ChekingForMissingFiles:
					bindString("step-status", Wt::WString::tr("Lms.Admin.ScannerController.step-checking-for-missing-files")
						.arg(status.currentScanStepStats->progress()));
					break;

				case Scanner::ScanProgressStep::DiscoveringFiles:
					bindString("step-status", Wt::WString::tr("Lms.Admin.ScannerController.step-discovering-files")
						.arg(status.currentScanStepStats->processedElems));
						break;

				case Scanner::ScanProgressStep::ScanningFiles:
					bindString("step-status", Wt::WString::tr("Lms.Admin.ScannerController.step-scanning-files")
						.arg(status.currentScanStepStats->processedElems)
						.arg(status.currentScanStepStats->totalElems)
						.arg(status.currentScanStepStats->progress()));
					break;

				case Scanner::ScanProgressStep::FetchingTrackFeatures:
					bindString("step-status", Wt::WString::tr("Lms.Admin.ScannerController.step-fetching-track-features")
						.arg(status.currentScanStepStats->processedElems)
						.arg(status.currentScanStepStats->totalElems)
						.arg(status.currentScanStepStats->progress()));
					break;
				case Scanner::ScanProgressStep::ReloadingSimilarityEngine:
					bindString("step-status", Wt::WString::tr("Lms.Admin.ScannerController.step-reloading-similarity-engine")
						.arg(status.currentScanStepStats->progress()));
					break;
			}
			break;
	}

}

} // namespace UserInterface

