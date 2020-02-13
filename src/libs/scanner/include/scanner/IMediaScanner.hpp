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

#include <Wt/WDateTime.h>
#include <Wt/WSignal.h>

#include "MediaScannerAddon.hpp"
#include "MediaScannerStats.hpp"

namespace Database
{
	class Db;
}

namespace Scanner {

class IMediaScanner
{
	public:
		virtual ~IMediaScanner() = default;

		virtual void setAddon(MediaScannerAddon& addon) = 0;

		virtual void start() = 0;
		virtual void stop() = 0;
		virtual void restart() = 0;

		// Async requests
		virtual void requestImmediateScan() = 0;
		virtual void requestReschedule() = 0;


		enum class State
		{
			NotScheduled,
			Scheduled,
			InProgress,
		};

		struct Status
		{
			State					currentState {State::NotScheduled};
			Wt::WDateTime				nextScheduledScan;
			std::optional<ScanStats>		lastCompleteScanStats;
			std::optional<ScanProgressStats>	inProgressScanStats;
		};

		virtual Status getStatus() = 0;

		// Called just after scan complete
		virtual Wt::Signal<>& scanComplete() = 0;

		// Called during scan in progress
		virtual Wt::Signal<ScanProgressStats>& scanInProgress() = 0;

		// Called after a schedule
		virtual Wt::Signal<Wt::WDateTime>& scheduled() = 0;

};

std::unique_ptr<IMediaScanner> createMediaScanner(Database::Db& db);


} // Scanner

