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

#include "ScanStepBase.hpp"

namespace Scanner
{
	class ScanStepRemoveOrphanDbFiles : public ScanStepBase
	{
		public:
			using ScanStepBase::ScanStepBase;

		private:
			std::string_view getStepName() const override { return "Checking orphaned entries"; }
			ScanStep getStep() const override { return ScanStep::ChekingForMissingFiles; }
			void process(ScanContext& context) override;

			void removeOrphanTracks(ScanContext& context);
			void removeOrphanClusters();
			void removeOrphanArtists();
			void removeOrphanReleases();
			bool checkFile(const std::filesystem::path& p);
	};
}
