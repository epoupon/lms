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

#include "ScanStepScanFiles.hpp"

#include "FileScanQueue.hpp"
#include "ScannerSettings.hpp"
#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "core/Path.hpp"
#include "database/Db.hpp"
#include "database/Session.hpp"
#include "scanners/FileToScan.hpp"
#include "scanners/IFileScanOperation.hpp"
#include "scanners/IFileScanner.hpp"

#include "ScanContext.hpp"

namespace lms::scanner
{
    using namespace db;

    namespace
    {
        std::size_t getScanMetaDataThreadCount()
        {
            std::size_t threadCount{ core::Service<core::IConfig>::get()->getULong("scanner-metadata-thread-count", 0) };

            if (threadCount == 0)
                threadCount = std::max<std::size_t>(std::thread::hardware_concurrency() / 2, 1);

            return threadCount;
        }

        FileToScan retrieveFileInfo(const std::filesystem::path& file, const MediaLibraryInfo& mediaLibrary, std::error_code& ec)
        {
            FileToScan res;

            res.lastWriteTime = core::pathUtils::getLastWriteTime(file, ec);
            if (!ec)
                res.relativePath = std::filesystem::relative(file, mediaLibrary.rootDirectory, ec);
            if (!ec)
                res.fileSize = std::filesystem::file_size(file, ec);

            if (!ec)
            {
                res.filePath = file;
                res.mediaLibrary = mediaLibrary;
            }

            return res;
        }
    } // namespace

    ScanStepScanFiles::ScanStepScanFiles(InitParams& initParams)
        : ScanStepBase{ initParams }
        , _fileScanQueue{ getScanMetaDataThreadCount(), _abortScan }
    {
        visitFileScanners([](IFileScanner* scanner) {
            for (const std::filesystem::path& file : scanner->getSupportedFiles())
                LMS_LOG(DBUPDATER, INFO, scanner->getName() << ": supporting file " << file);

            for (const std::filesystem::path& extension : scanner->getSupportedExtensions())
                LMS_LOG(DBUPDATER, INFO, scanner->getName() << ": supporting file extension " << extension);
        });

        LMS_LOG(DBUPDATER, INFO, "Using " << _fileScanQueue.getThreadCount() << " thread(s) for scanning file metadata");
    }

    bool ScanStepScanFiles::needProcess([[maybe_unused]] const ScanContext& context) const
    {
        // Always need to scan files
        return true;
    }

    void ScanStepScanFiles::process(ScanContext& context)
    {
        context.currentStepStats.totalElems = context.stats.totalFileCount;

        for (const MediaLibraryInfo& mediaLibrary : _settings.mediaLibraries)
            process(context, mediaLibrary);
    }

    void ScanStepScanFiles::process(ScanContext& context, const MediaLibraryInfo& mediaLibrary)
    {
        const std::size_t scanQueueMaxScanRequestCount{ 100 * _fileScanQueue.getThreadCount() };
        const std::size_t processFileResultsBatchSize{ 10 };

        std::vector<std::unique_ptr<IFileScanOperation>> scanOperations;

        core::pathUtils::exploreFilesRecursive(
            mediaLibrary.rootDirectory, [&](std::error_code ec, const std::filesystem::path& path) {
                LMS_SCOPED_TRACE_DETAILED("Scanner", "OnExploreFile");

                if (_abortScan)
                    return false; // stop iterating

                if (ec)
                {
                    addError<IOScanError>(context, path, ec);
                    context.stats.skips++;
                }
                else if (IFileScanner * scanner{ selectFileScanner(path) })
                {
                    FileToScan fileToScan{ retrieveFileInfo(path, mediaLibrary, ec) };
                    if (ec)
                    {
                        addError<IOScanError>(context, path, ec);
                        context.stats.skips++;
                    }
                    else
                    {
                        if (context.scanOptions.fullScan || scanner->needsScan(fileToScan))
                        {
                            auto scanOperation{ scanner->createScanOperation(std::move(fileToScan)) };
                            _fileScanQueue.pushScanRequest(std::move(scanOperation));
                        }
                    }

                    context.currentStepStats.processedElems++;
                    _progressCallback(context.currentStepStats);
                }

                while (_fileScanQueue.getResultsCount() > (scanQueueMaxScanRequestCount / 2))
                {
                    _fileScanQueue.popResults(scanOperations, processFileResultsBatchSize);
                    processFileScanResults(context, scanOperations);
                }

                _fileScanQueue.wait(scanQueueMaxScanRequestCount);

                return true;
            },
            &excludeDirFileName);

        _fileScanQueue.wait();

        while (!_abortScan && _fileScanQueue.popResults(scanOperations, processFileResultsBatchSize) > 0)
            processFileScanResults(context, scanOperations);
    }

    void ScanStepScanFiles::processFileScanResults(ScanContext& context, std::span<std::unique_ptr<IFileScanOperation>> scanOperations)
    {
        LMS_SCOPED_TRACE_OVERVIEW("Scanner", "ProcessScanResults");

        db::Session& dbSession{ _db.getTLSSession() };
        auto transaction{ dbSession.createWriteTransaction() };

        for (auto& scanOperation : scanOperations)
        {
            if (_abortScan)
                return;

            LMS_LOG(DBUPDATER, DEBUG, scanOperation->getName() << ": processing result for " << scanOperation->getFilePath());
            const IFileScanOperation::OperationResult res{ scanOperation->processResult() };
            switch (res)
            {
            case IFileScanOperation::OperationResult::Added:
                context.stats.additions++;
                break;
            case IFileScanOperation::OperationResult::Removed:
                context.stats.deletions++;
                break;
            case IFileScanOperation::OperationResult::Skipped:
                context.stats.failures++;
                break;
            case IFileScanOperation::OperationResult::Updated:
                context.stats.updates++;
                break;
            }
            context.stats.scans++;

            for (const auto& error : scanOperation->getErrors())
                addError(context, error);
        }
    }
} // namespace lms::scanner