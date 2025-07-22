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

#include <deque>

#include "ScannerSettings.hpp"
#include "core/IJob.hpp"
#include "core/IJobScheduler.hpp"
#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "scanners/FileToScan.hpp"
#include "scanners/IFileScanOperation.hpp"
#include "scanners/IFileScanner.hpp"

#include "FileScanners.hpp"
#include "JobQueue.hpp"
#include "ScanContext.hpp"

namespace lms::scanner
{
    namespace
    {
        using ExploreFileCallback = std::function<bool(std::error_code, const std::filesystem::path& path, const std::filesystem::directory_entry*)>;
        bool exploreFilesRecursive(const std::filesystem::path& directory, ExploreFileCallback cb, const std::filesystem::path* excludeDirFileName)
        {
            std::error_code ec;
            std::filesystem::directory_iterator itPath{ directory, std::filesystem::directory_options::follow_directory_symlink, ec };

            if (ec)
            {
                cb(ec, directory, nullptr);
                return true; // try to continue exploring anyway
            }

            if (excludeDirFileName && !excludeDirFileName->empty())
            {
                const std::filesystem::path excludePath{ directory / *excludeDirFileName };
                if (std::filesystem::exists(excludePath, ec))
                {
                    LMS_LOG(DBUPDATER, DEBUG, "Found " << excludePath << ": skipping directory");
                    return true;
                }
            }

            std::filesystem::directory_iterator itEnd;
            while (itPath != itEnd)
            {
                bool continueExploring{ true };

                const std::filesystem::directory_entry& entry{ *itPath };
                const std::filesystem::path& path{ entry.path() };

                if (ec)
                {
                    continueExploring = cb(ec, path, nullptr);
                }
                else
                {
                    if (entry.is_regular_file())
                        continueExploring = cb(ec, path, &entry);
                    else if (entry.is_directory())
                        continueExploring = exploreFilesRecursive(path, cb, excludeDirFileName);
                }

                if (!continueExploring)
                    return false;

                itPath.increment(ec);
            }

            return true;
        }

        class FileScanJob : public core::IJob
        {
        public:
            FileScanJob(const FileScanners& fileScanners, const MediaLibraryInfo& mediaLibrary, bool fullScan, std::span<const std::filesystem::directory_entry> files)
                : _fileScanners{ fileScanners }
                , _mediaLibrary{ mediaLibrary }
                , _fullScan{ fullScan }
                , _processCount{}
                , _skipCount{}
                , _files{ std::cbegin(files), std::cend(files) }
            {
            }

            std::size_t getFileCount() const { return _processCount; };
            std::size_t getSkipCount() const { return _skipCount; }

            std::span<std::unique_ptr<IFileScanOperation>> getScanOperations()
            {
                return _scanOperations;
            }

        private:
            core::LiteralString getName() const override
            {
                return "Scan Files";
            }

            void run() override
            {
                for (const auto& file : _files)
                {
                    IFileScanner* scanner{ _fileScanners.select(file.path()) };
                    if (!scanner)
                        continue;

                    FileToScan fileToScan;
                    fileToScan.filePath = file.path();
                    fileToScan.mediaLibrary = _mediaLibrary;
                    {
                        const std::chrono::system_clock::time_point lastWriteTime{ std::chrono::time_point_cast<std::chrono::system_clock::duration>(std::chrono::file_clock::to_sys(file.last_write_time())) };
                        fileToScan.lastWriteTime.setTime_t(std::chrono::system_clock::to_time_t(lastWriteTime)); // sec resolution, as stored in the database
                    }
                    fileToScan.fileSize = file.file_size();

                    if (_fullScan || scanner->needsScan(fileToScan))
                    {
                        auto scanOperation{ scanner->createScanOperation(std::move(fileToScan)) };

                        {
                            LMS_SCOPED_TRACE_DETAILED("Scanner", scanOperation->getName());
                            scanOperation->scan();
                        }
                        _scanOperations.push_back(std::move(scanOperation));
                    }
                    else
                        _skipCount++;

                    _processCount++;
                }
            }

            const FileScanners& _fileScanners;
            const MediaLibraryInfo& _mediaLibrary;
            const bool _fullScan;
            std::size_t _processCount;
            std::size_t _skipCount;
            std::vector<std::filesystem::directory_entry> _files;
            std::vector<std::unique_ptr<IFileScanOperation>> _scanOperations;
        };
    } // namespace

    bool ScanStepScanFiles::needProcess([[maybe_unused]] const ScanContext& context) const
    {
        // Always need to scan files
        return true;
    }

    void ScanStepScanFiles::process(ScanContext& context)
    {
        for (const MediaLibraryInfo& mediaLibrary : _settings.mediaLibraries)
            process(context, mediaLibrary);

        context.stats.totalFileCount = context.currentStepStats.processedElems;
    }

    void ScanStepScanFiles::process(ScanContext& context, const MediaLibraryInfo& mediaLibrary)
    {
        constexpr std::size_t filesPerScanJob{ 10 };
        constexpr std::size_t scanQueueMaxSize{ 50 };
        constexpr std::size_t processFileResultsBatchSize{ 1 };
        constexpr float drainRatio{ 0.85 };

        std::deque<std::unique_ptr<IFileScanOperation>> operations;

        auto processDoneJobs = [&](std::span<std::unique_ptr<core::IJob>> jobsDone) {
            for (const auto& jobDone : jobsDone)
            {
                auto& fileScanJob{ static_cast<FileScanJob&>(*jobDone) };
                for (std::unique_ptr<IFileScanOperation>& scanOperation : fileScanJob.getScanOperations())
                    operations.push_back(std::move(scanOperation));

                context.currentStepStats.processedElems += fileScanJob.getFileCount();
                context.stats.skips += fileScanJob.getSkipCount();
            }

            if (!_abortScan)
                processFileScanOperations(context, operations, true /* force batch */);

            _progressCallback(context.currentStepStats);
        };

        {
            JobQueue queue{ getJobScheduler(), scanQueueMaxSize, processDoneJobs, processFileResultsBatchSize, drainRatio };

            std::vector<std::filesystem::directory_entry> filesToScan;

            exploreFilesRecursive(
                mediaLibrary.rootDirectory, [&](std::error_code ec, const std::filesystem::path& path, const std::filesystem::directory_entry* fileEntry) {
                    LMS_SCOPED_TRACE_DETAILED("Scanner", "OnExploreFile");

                    assert((ec && !fileEntry) || (!ec && fileEntry));

                    if (_abortScan)
                        return false; // stop iterating

                    if (ec)
                    {
                        addError<IOScanError>(context, path, ec);
                        context.stats.skips++;
                    }
                    else
                    {
                        filesToScan.push_back(*fileEntry);

                        if (filesToScan.size() >= filesPerScanJob)
                        {
                            queue.push(std::make_unique<FileScanJob>(getFileScanners(), mediaLibrary, context.scanOptions.fullScan, filesToScan));
                            filesToScan.clear();
                        }
                    }

                    return true;
                },
                &excludeDirFileName);

            if (!filesToScan.empty())
                queue.push(std::make_unique<FileScanJob>(getFileScanners(), mediaLibrary, context.scanOptions.fullScan, filesToScan));

            _progressCallback(context.currentStepStats);
        }

        // Process remaining objects
        processFileScanOperations(context, operations, false /* force batch */);
    }

    std::size_t ScanStepScanFiles::processFileScanOperations(ScanContext& context, std::deque<std::unique_ptr<IFileScanOperation>>& scanOperations, bool forceBatch)
    {
        std::size_t count{};
        constexpr std::size_t writeBatchSize{ 10 };

        LMS_SCOPED_TRACE_OVERVIEW("Scanner", "ProcessScanResults");

        while ((forceBatch && scanOperations.size() >= writeBatchSize) || !scanOperations.empty())
        {
            db::Session& dbSession{ _db.getTLSSession() };
            auto transaction{ dbSession.createWriteTransaction() };

            for (std::size_t i{}; !scanOperations.empty() && i < writeBatchSize; ++i)
            {
                processFileScanOperation(context, *scanOperations.front());
                scanOperations.pop_front();
                count++;
            }
        }

        return count;
    }

    void ScanStepScanFiles::processFileScanOperation(ScanContext& context, IFileScanOperation& scanOperation)
    {
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
} // namespace lms::scanner