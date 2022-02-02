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

#include "ScannerEvents.hpp"
#include "ScannerStats.hpp"

namespace Database
{
	class Db;
}

namespace Recommendation
{
	class IRecommendationService;
}

namespace Scanner
{

	class IScannerService
	{
		public:
			virtual ~IScannerService() = default;

			// Async requests
			virtual void requestReload() = 0;
			virtual void requestImmediateScan(bool force) = 0;

			enum class State
			{
				NotScheduled,
				Scheduled,
				InProgress,
			};

			struct Status
			{
				State								currentState {State::NotScheduled};
				Wt::WDateTime						nextScheduledScan;
				std::optional<ScanStats>			lastCompleteScanStats;
				std::optional<ScanStepStats> 		currentScanStepStats;
			};

			virtual Status getStatus() const = 0;

			virtual Events& getEvents() = 0;
	};

	std::unique_ptr<IScannerService> createScannerService(Database::Db& db, Recommendation::IRecommendationService& recommendationEngine);

} // Scanner

