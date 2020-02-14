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
#include <mutex>
#include <optional>

#include <Wt/WDateTime.h>
#include <Wt/WIOService.h>
#include <Wt/WSignal.h>

#include <boost/asio/system_timer.hpp>

#include "database/ScanSettings.hpp"
#include "database/Session.hpp"
#include "metadata/TagLibParser.hpp"
#include "scanner/IMediaScanner.hpp"


namespace Scanner {

class MediaScanner : public IMediaScanner
{
	public:
		MediaScanner(Database::Db& db);

		void setAddon(MediaScannerAddon& addon) override;

		void start() override;
		void stop() override;
		void restart() override;

		void requestImmediateScan() override;
		void requestReschedule() override ;

		Status getStatus() override;

		Wt::Signal<>& scanComplete() override { return _sigScanComplete; }
		Wt::Signal<ScanProgressStats>& scanInProgress() override { return _sigScanInProgress; }
		Wt::Signal<Wt::WDateTime>& scheduled() override { return _sigScheduled; }

	private:

		// Job handling
		void scheduleNextScan();
		void scheduleScan(const Wt::WDateTime& dateTime = {});

		// Update database (scheduled callback)
		void scan(boost::system::error_code ec);

		void scanMediaDirectory( const std::filesystem::path& mediaDirectory, bool forceScan, ScanStats& stats);

		// Helpers
		void refreshScanSettings();

		void countAllFiles(ScanStats& stats);
		void removeMissingTracks(ScanStats& stats);
		void removeOrphanEntries();
		void checkDuplicatedAudioFiles(ScanStats& stats);
		void scanAudioFile(const std::filesystem::path& file, bool forceScan, ScanStats& stats);
		Database::IdType doScanAudioFile(const std::filesystem::path& file, ScanStats& stats);
		void notifyInProgressIfNeeded(const ScanStats& stats);
		void notifyInProgress(const ScanStats& stats);

		bool					_running {false};
		Wt::WIOService				_ioService;
		boost::asio::system_timer		_scheduleTimer {_ioService};
		Wt::Signal<>				_sigScanComplete;
		Wt::Signal<ScanProgressStats>		_sigScanInProgress;
		std::chrono::system_clock::time_point	_lastScanInProgressEmit {};
		Wt::Signal<Wt::WDateTime>		_sigScheduled;
		Database::Session			_dbSession;
		MetaData::TagLibParser 			_metadataParser;
		std::vector<MediaScannerAddon*> 	_addons;

		std::mutex				_statusMutex;
		State					_curState {State::NotScheduled};
		std::optional<ScanStats> 		_lastCompleteScanStats;
		std::optional<ScanProgressStats> 	_inProgressScanStats;
		Wt::WDateTime				_nextScheduledScan;

		// Current scan settings
		std::size_t _scanVersion {};
		Wt::WTime _startTime;
		Database::ScanSettings::UpdatePeriod 	_updatePeriod {Database::ScanSettings::UpdatePeriod::Never};
		std::set<std::filesystem::path>		_fileExtensions;
		std::filesystem::path			_mediaDirectory;


}; // class MediaScanner

} // Scanner

