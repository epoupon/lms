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

#include <Wt/WDateTime.h>
#include <Wt/WIOService.h>
#include <Wt/WSignal.h>

#include <boost/asio/system_timer.hpp>

#include "database/ScanSettings.hpp"
#include "database/DatabaseHandler.hpp"
#include "metadata/TagLibParser.hpp"

#include "MediaScannerAddon.hpp"

namespace Scanner {

class MediaScanner
{
	public:
		MediaScanner(Wt::Dbo::SqlConnectionPool& connectionPool);

		void setAddon(MediaScannerAddon& addon);

		void start();
		void stop();
		void restart();

		// Async requests
		void requestImmediateScan();
		void requestReschedule();

		struct Stats
		{
			Wt::WDateTime	startTime;
			Wt::WDateTime	stopTime;
			std::size_t	skips = 0;		// no change since last scan
			std::size_t	scans = 0;		// actually scanned filed
			std::size_t	scanErrors = 0;		// cannot scan file
			std::size_t	incompleteScans = 0;	// Scanned, but not imported (criteria not filled)
			std::size_t	additions = 0;		// Added in DB
			std::size_t	deletions = 0;		// removed from DB
			std::size_t	updates = 0;		// updated file in DB

			std::size_t	duplicateHashes = 0;	// Same file hashes
			std::size_t	duplicateMBID = 0;	// Same MBID

			std::size_t	totalFiles = 0;		// Total number of files to be scanned

			std::size_t nbFiles() const { return skips + additions + updates; }
			std::size_t nbChanges() const { return additions + deletions + updates; }
			std::size_t nbErrors() const { return scanErrors + incompleteScans; }
			std::size_t nbDuplicates() const { return duplicateHashes + duplicateMBID; }
			float progress() const { return (nbFiles() - totalFiles) / static_cast<float>(totalFiles); }
		};

		enum class State
		{
			NotScheduled,
			Scheduled,
			InProgress,
		};

		struct Status
		{
			State				currentState {State::NotScheduled};
			Wt::WDateTime			nextScheduledScan;
			boost::optional<Stats>		lastScanStats;
			boost::optional<Stats>		inProgressStats;
		};

		Status getStatus();

		// Called just after scan complete
		Wt::Signal<Stats>& scanComplete() { return _sigScanComplete; }

		// Called during scan in progress
		Wt::Signal<Stats>& scanInProgress() { return _sigScanInProgress; }

		// Called after a schedule
		Wt::Signal<Wt::WDateTime>& scheduled() { return _sigScheduled; }

	private:

		// Job handling
		void scheduleNextScan();
		void scheduleScan(const Wt::WDateTime& dateTime = {});

		// Update database (scheduled callback)
		void scan(boost::system::error_code ec);

		void scanMediaDirectory( boost::filesystem::path mediaDirectory, bool forceScan, Stats& stats);

		// Helpers
		void refreshScanSettings();

		void countAllFiles(Stats& stats);
		void removeMissingTracks(Stats& stats);
		void removeOrphanEntries();
		void checkDuplicatedAudioFiles(Stats& stats);
		void scanAudioFile(const boost::filesystem::path& file, bool forceScan, Stats& stats);
		void notifyInProgressIfNeeded(Stats& stats);

		bool			_running {false};
		Wt::WIOService		_ioService;
		boost::asio::system_timer _scheduleTimer {_ioService};
		Wt::Signal<Stats>	_sigScanComplete;
		Wt::Signal<Stats>	_sigScanInProgress;
		std::chrono::system_clock::time_point _lastScanInProgressEmit {};
		Wt::Signal<Wt::WDateTime> _sigScheduled;
		Database::Handler	_db;
		MetaData::TagLibParser 	_metadataParser;
		std::vector<MediaScannerAddon*> _addons;

		std::mutex		_statusMutex;
		State			_curState {State::NotScheduled};
		boost::optional<Stats>	_inProgressStats;
		boost::optional<Stats> 	_lastScanStats;
		Wt::WDateTime		_nextScheduledScan;

		// Current scan settings
		std::size_t _scanVersion {};
		Wt::WTime _startTime;
		Database::ScanSettings::UpdatePeriod 	_updatePeriod {Database::ScanSettings::UpdatePeriod::Never};
		std::set<boost::filesystem::path>	_fileExtensions;
		boost::filesystem::path			_mediaDirectory;


}; // class MediaScanner

} // Scanner

