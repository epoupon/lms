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

#include <filesystem>
#include <set>
#include <string>
#include <vector>
#include <Wt/WDateTime.h>
#include "services/database/ScanSettings.hpp"

namespace Scanner
{
	struct ScannerSettings
	{
		std::size_t											scanVersion {};
		Wt::WTime											startTime;
		Database::ScanSettings::UpdatePeriod 				updatePeriod {Database::ScanSettings::UpdatePeriod::Never};
		std::vector<std::filesystem::path>					supportedExtensions;
		Database::ScanSettings::RecommendationEngineType	recommendationServiceType;
		std::filesystem::path								mediaDirectory;
		bool												skipDuplicateMBID {};
		std::set<std::string>								clusterTypeNames;

		bool operator==(const ScannerSettings& rhs) const
		{
			return scanVersion == rhs.scanVersion
				&& startTime == rhs.startTime
				&& updatePeriod == rhs.updatePeriod
				&& supportedExtensions == rhs.supportedExtensions
				&& recommendationServiceType == rhs.recommendationServiceType
				&& mediaDirectory == rhs.mediaDirectory
				&& skipDuplicateMBID == rhs.skipDuplicateMBID
				&& clusterTypeNames == rhs.clusterTypeNames;
		}
	};
}
