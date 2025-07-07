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

#include "ScannerSettings.hpp"
#include "core/IJob.hpp"
#include "core/IJobScheduler.hpp"
#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "core/Path.hpp"
#include "database/Db.hpp"
#include "database/Session.hpp"
#include "scanners/FileToScan.hpp"
#include "scanners/IFileScanOperation.hpp"
#include "scanners/IFileScanner.hpp"

#include "JobQueue.hpp"
#include "ScanContext.hpp"

namespace lms::scanner
{
    using namespace db;

    namespace
    {
        class FileScanJob : public core::IJob
        {
        public:
            FileScanJob(std::unique_ptr<IFileScanOperation> scanOperation)
                : _scanOperation{ std::move(scanOperation) }
            {
            }

            IFileScanOperation& getScanOperation()
            {
                return *_scanOperation;
            }

        private:
            core::LiteralString getName() const override
            {
                return _scanOperation->getName();
            }

            void run() override
            {
                _scanOperation->scan();
            }

            std::unique_ptr<IFileScanOperation> _scanOperation;
        };
    } // namespace

    ScanStepScanFiles::ScanStepScanFiles(InitParams& initParams)
        : ScanStepBase{ initParams }
    {
        visitFileScanners([](IFileScanner* scanner) {
            for (const std::filesystem::path& file : scanner->getSupportedFiles())
                LMS_LOG(DBUPDATER, INFO, scanner->getName() << ": supporting file " << file);

            for (const std::filesystem::path& extension : scanner->getSupportedExtensions())
                LMS_LOG(DBUPDATER, INFO, scanner->getName() << ": supporting file extension " << extension);
        });
    }

    bool ScanStepScanFiles::needProcess([[maybe_unused]] const ScanContext& context) const
    {
        // Always need to scan files
        return true;
    }

    void ScanStepScanFiles::process(ScanContext& context)
    {
        for (const MediaLibraryInfo& mediaLibrary : _settings.mediaLibraries)
            process(context, mediaLibrary);
    }

    void ScanStepScanFiles::process(ScanContext& context, const MediaLibraryInfo& mediaLibrary)
    {
        const std::size_t scanQueueMaxScanRequestCount{ 50 * getJobScheduler().getThreadCount() };
        constexpr std::size_t processFileResultsBatchSize{ 10 };
        constexpr float drainRatio{ 0.85 };

        auto processDoneJobs = [&](std::span<std::unique_ptr<core::IJob>> jobsDone) {
            if (!_abortScan)
                processFileScanResults(context, jobsDone);
        };

        JobQueue queue{ getJobScheduler(), scanQueueMaxScanRequestCount, processDoneJobs, processFileResultsBatchSize, drainRatio };

        std::vector<std::unique_ptr<core::IJob>> jobsDone;
        std::vector<std::unique_ptr<IFileScanOperation>> scanOperations;

        core::pathUtils::exploreFilesRecursive(
            mediaLibrary.rootDirectory, [&](std::error_code ec, const std::filesystem::path& path, const core::pathUtils::FileInfo* fileInfo) {
                LMS_SCOPED_TRACE_DETAILED("Scanner", "OnExploreFile");

                assert((ec && !fileInfo) || (!ec && fileInfo));

                if (_abortScan)
                    return false; // stop iterating

                if (ec)
                {
                    addError<IOScanError>(context, path, ec);
                    context.stats.skips++;
                }
                else if (IFileScanner * scanner{ selectFileScanner(path) })
                {
                    FileToScan fileToScan;

                    fileToScan.filePath = path;
                    fileToScan.mediaLibrary = mediaLibrary;
                    fileToScan.lastWriteTime.setTime_t(fileInfo->lastWriteTime.toTime_t()); // sec resolution, as stored in the database
                    fileToScan.fileSize = fileInfo->fileSize;

                    if (context.scanOptions.fullScan || scanner->needsScan(fileToScan))
                    {
                        auto scanOperation{ scanner->createScanOperation(std::move(fileToScan)) };
                        queue.push(std::make_unique<FileScanJob>(std::move(scanOperation)));
                    }

                    context.currentStepStats.processedElems++;
                    _progressCallback(context.currentStepStats);
                }

                return true;
            },
            &excludeDirFileName);

        context.stats.totalFileCount = context.currentStepStats.totalElems;
    }

    void ScanStepScanFiles::processFileScanResults(ScanContext& context, std::span<std::unique_ptr<core::IJob>> scanJobs)
    {
        LMS_SCOPED_TRACE_OVERVIEW("Scanner", "ProcessScanResults");

        db::Session& dbSession{ _db.getTLSSession() };
        auto transaction{ dbSession.createWriteTransaction() };

        for (auto& scanJob : scanJobs)
        {
            IFileScanOperation& scanOperation{ static_cast<FileScanJob&>(*scanJob).getScanOperation() };

            LMS_LOG(DBUPDATER, DEBUG, scanOperation.getName() << ": processing result for " << scanOperation.getFilePath());
            const IFileScanOperation::OperationResult res{ scanOperation.processResult() };
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

            for (const auto& error : scanOperation.getErrors())
                addError(context, error);
        }
    }
} // namespace lms::scanner