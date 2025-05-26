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

#pragma once

#include <filesystem>
#include <memory>
#include <vector>

#include "core/LiteralString.hpp"

namespace lms::scanner
{
    struct ScanError;

    class IFileScanOperation
    {
    public:
        virtual ~IFileScanOperation() = default;

        virtual core::LiteralString getName() const = 0;

        virtual const std::filesystem::path& getFilePath() const = 0;

        // scan() is called asynchronously by a pool of threads
        // processResult() is called sequentially by a single thread
        virtual void scan() = 0;

        enum class OperationResult
        {
            Added,
            Removed,
            Updated,
            Skipped,
        };
        virtual OperationResult processResult() = 0;

        using ScanErrorVector = std::vector<std::shared_ptr<ScanError>>;
        // list of errors collected during scan/result processing (there might be errors without skipping the file)
        virtual const ScanErrorVector& getErrors() = 0;
    };
} // namespace lms::scanner