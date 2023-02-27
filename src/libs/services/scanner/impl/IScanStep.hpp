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

#include <string_view>

#include "services/scanner/ScannerStats.hpp"

namespace Scanner
{
	class IScanStep
	{
		public:
			virtual ~IScanStep() = default;

			virtual ScanStep getStep() const = 0;
			virtual std::string_view getStepName() const = 0;

			struct ScanContext
			{
				const std::filesystem::path directory;
				const bool forceScan;
				ScanStats stats;
				ScanStepStats currentStepStats;
			};
			virtual void process(ScanContext& context) = 0;
	};
}
