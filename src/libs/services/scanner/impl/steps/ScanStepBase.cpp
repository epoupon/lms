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

#include "core/String.hpp"

#include "ScanContext.hpp"
#include "scanners/IFileScanner.hpp"

namespace lms::scanner
{
    ScanStepBase::ScanStepBase(InitParams& initParams)
        : _settings{ initParams.settings }
        , _progressCallback{ initParams.progressCallback }
        , _abortScan{ initParams.abortScan }
        , _db{ initParams.db }
        , _fileScanners(std::cbegin(initParams.fileScanners), std::cend(initParams.fileScanners))
        , _lastScanSettings{ initParams.lastScanSettings }
    {
        for (IFileScanner* scanner : _fileScanners)
        {
            for (const std::filesystem::path& file : scanner->getSupportedFiles())
            {
                [[maybe_unused]] auto [it, inserted]{ _scannerByFile.emplace(file, scanner) };
                assert(inserted);
            }

            for (const std::filesystem::path& extension : scanner->getSupportedExtensions())
            {
                [[maybe_unused]] auto [it, inserted]{ _scannerByExtension.emplace(extension, scanner) };
                assert(inserted);
            }
        }
    }

    ScanStepBase::~ScanStepBase() = default;

    IFileScanner* ScanStepBase::selectFileScanner(const std::filesystem::path& filePath) const
    {
        {
            const std::string fileName{ core::stringUtils::stringToLower(filePath.filename().string()) };

            auto itScanner{ _scannerByFile.find(fileName) };
            if (itScanner != std::cend(_scannerByFile))
                return itScanner->second;
        }

        {
            const std::string extension{ core::stringUtils::stringToLower(filePath.extension().string()) };

            auto itScanner{ _scannerByExtension.find(extension) };
            if (itScanner != std::cend(_scannerByExtension))
                return itScanner->second;
        }

        return nullptr;
    }

    void ScanStepBase::visitFileScanners(const std::function<void(IFileScanner*)>& visitor) const
    {
        for (IFileScanner* scanner : _fileScanners)
            visitor(scanner);
    }

    void ScanStepBase::addError(ScanContext& context, std::shared_ptr<ScanError> error)
    {
        error->accept(_scanErrorLogger);

        context.stats.errorsCount++;

        if (context.stats.errors.size() < ScanStats::maxStoredErrorCount)
            context.stats.errors.emplace_back(error);
    }
} // namespace lms::scanner
