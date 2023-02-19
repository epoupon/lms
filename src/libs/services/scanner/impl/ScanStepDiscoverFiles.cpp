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

#include "ScanStepDiscoverFiles.hpp"
#include "utils/Logger.hpp"
#include "utils/Path.hpp"

namespace Scanner
{
	void
	ScanStepDiscoverFiles::process(ScanContext& context)
	{
		context.stats.filesScanned = 0;
		PathUtils::exploreFilesRecursive(context.directory, [&](std::error_code ec, const std::filesystem::path& path)
		{
			if (_abortScan)
				return false;

			if (!ec && PathUtils::hasFileAnyExtension(path, _settings.supportedExtensions))
			{
				context.currentStepStats.processedElems++;
				_progressCallback(context.currentStepStats);
			}

			return true;
		}, &excludeDirFileName);

		context.stats.filesScanned = context.currentStepStats.processedElems;

		LMS_LOG(DBUPDATER, DEBUG) << "Discovered " << context.stats.filesScanned << " files in '" << context.directory << "'";
	}
}
