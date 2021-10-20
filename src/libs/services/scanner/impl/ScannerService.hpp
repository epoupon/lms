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
#include <shared_mutex>
#include <optional>
#include <unordered_set>

#include <Wt/WDateTime.h>
#include <Wt/WIOService.h>
#include <Wt/WSignal.h>

#include <boost/asio/system_timer.hpp>

#include "database/Types.hpp"
#include "database/ScanSettings.hpp"
#include "database/Session.hpp"
#include "metadata/IParser.hpp"
#include "services/scanner/IScannerService.hpp"
#include "utils/Path.hpp"

class UUID;

namespace Recommendation
{
	class IRecommendationService;
}

namespace Scanner
{
	class ScannerService : public IScannerService
	{
		public:
			ScannerService(Database::Db& db, Recommendation::IRecommendationService& recommendationService);
			~ScannerService();

			ScannerService(const ScannerService&) = delete;
			ScannerService(ScannerService&&) = delete;
			ScannerService& operator=(const ScannerService&) = delete;
			ScannerService& operator=(ScannerService&&) = delete;

			void requestReload() override;
			void requestImmediateScan(bool force) override;

			Status	getStatus() const override;
			Events&	getEvents() override { return _events; }

		private:
			void start();
			void stop();

			// Job handling
			void scheduleNextScan();
			void scheduleScan(bool force, const Wt::WDateTime& dateTime = {});

			void abortScan();

			// Update database (scheduled callback)
			void scan(bool force);

			void scanMediaDirectory( const std::filesystem::path& mediaDirectory, bool forceScan, ScanStats& stats);
			bool fetchTrackFeatures(Database::TrackId trackId, const UUID& MBID);
			void fetchTrackFeatures(ScanStats& stats);

			// Helpers
			void refreshScanSettings();

			void countAllFiles(ScanStats& stats);
			void removeMissingTracks(ScanStats& stats);
			void removeOrphanEntries();
			void checkDuplicatedAudioFiles(ScanStats& stats);
			void scanAudioFile(const std::filesystem::path& file, bool forceScan, ScanStats& stats);
			void notifyInProgressIfNeeded(const ScanStepStats& stats);
			void notifyInProgress(const ScanStepStats& stats);
			void reloadSimilarityEngine(ScanStats& stats);

			Recommendation::IRecommendationService&	_recommendationService;

			std::mutex								_controlMutex;
			std::atomic<bool>						_abortScan {};
			Wt::WIOService							_ioService;
			boost::asio::system_timer				_scheduleTimer {_ioService};
			Events									_events;
			std::chrono::system_clock::time_point	_lastScanInProgressEmit {};
			Database::Session						_dbSession;
			std::unique_ptr<MetaData::IParser>		_metadataParser;

			mutable std::shared_mutex			_statusMutex;
			State								_curState {State::NotScheduled};
			std::optional<ScanStats> 			_lastCompleteScanStats;
			std::optional<ScanStepStats> 		_currentScanStepStats;
			Wt::WDateTime						_nextScheduledScan;

			// Current scan settings
			std::size_t				_scanVersion {};
			Wt::WTime				_startTime;
			Database::ScanSettings::UpdatePeriod 	_updatePeriod {Database::ScanSettings::UpdatePeriod::Never};
			std::unordered_set<std::filesystem::path>		_fileExtensions;
			std::filesystem::path					_mediaDirectory;
			Database::ScanSettings::RecommendationEngineType _recommendationServiceType;
	};
} // Scanner

