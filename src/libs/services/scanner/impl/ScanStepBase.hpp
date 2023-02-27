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

#include <functional>

#include "services/scanner/ScannerStats.hpp"
#include "IScanStep.hpp"
#include "ScannerSettings.hpp"

namespace Database
{
	class Db;
}

namespace Scanner
{
	class ScanStepBase : public IScanStep
	{
		public:
			static inline const std::filesystem::path excludeDirFileName {".lmsignore"};
			using ProgressCallback = std::function<void(const ScanStepStats& stats)>;

			struct InitParams
			{
				const ScannerSettings& settings;
				ProgressCallback progressCallback;
				bool& abortScan;
				Database::Db& db;
			};
			ScanStepBase(InitParams& initParams)
				: _settings {initParams.settings}
			, _progressCallback {initParams.progressCallback}
			, _abortScan {initParams.abortScan}
			, _db {initParams.db}
			{}

		protected:
			const ScannerSettings&	_settings;
			ProgressCallback		_progressCallback;
			bool&					_abortScan;
			Database::Db&			_db;
	};
}
