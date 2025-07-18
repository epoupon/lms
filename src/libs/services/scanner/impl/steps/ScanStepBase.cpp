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

#include "ScanStepBase.hpp"

#include "ScanContext.hpp"

namespace lms::scanner
{
    ScanStepBase::ScanStepBase(InitParams& initParams)
        : _settings{ initParams.settings }
        , _progressCallback{ initParams.progressCallback }
        , _abortScan{ initParams.abortScan }
        , _db{ initParams.db }
        , _jobScheduler{ initParams.jobScheduler }
        , _fileScanners(initParams.fileScanners)
        , _lastScanSettings{ initParams.lastScanSettings }
    {
    }

    ScanStepBase::~ScanStepBase() = default;

    void ScanStepBase::addError(ScanContext& context, std::shared_ptr<ScanError> error)
    {
        error->accept(_scanErrorLogger);

        context.stats.errorsCount++;

        if (context.stats.errors.size() < ScanStats::maxStoredErrorCount)
            context.stats.errors.emplace_back(error);
    }
} // namespace lms::scanner
