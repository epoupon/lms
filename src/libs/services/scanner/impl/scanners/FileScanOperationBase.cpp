/*
 * Copyright (C) 2024 Emeric Poupon
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

#include "FileScanOperationBase.hpp"

#include "services/scanner/ScanErrors.hpp"

namespace lms::scanner
{
    FileScanOperationBase::FileScanOperationBase(FileToScan&& fileToScan, db::IDb& db, const ScannerSettings& settings)
        : _file{ std::move(fileToScan) }
        , _db{ db }
        , _settings{ settings }
    {
    }

    FileScanOperationBase::~FileScanOperationBase() = default;
} // namespace lms::scanner