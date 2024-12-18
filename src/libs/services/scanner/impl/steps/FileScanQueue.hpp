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

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

#include "core/IOContextRunner.hpp"

namespace lms::scanner
{
    class IFileScanOperation;

    class FileScanQueue
    {
    public:
        FileScanQueue(std::size_t threadCount, bool& abort);
        ~FileScanQueue();
        FileScanQueue(const FileScanQueue&) = delete;
        FileScanQueue& operator=(const FileScanQueue&) = delete;

        std::size_t getThreadCount() const { return _scanContextRunner.getThreadCount(); }

        void pushScanRequest(std::unique_ptr<IFileScanOperation> operation);

        std::size_t getResultsCount() const;
        size_t popResults(std::vector<std::unique_ptr<IFileScanOperation>>& results, std::size_t maxCount);

        void wait(std::size_t maxScanRequestCount = 0); // wait until ongoing scan request count <= maxScanRequestCount

    private:
        boost::asio::io_context _scanIoContext;
        core::IOContextRunner _scanContextRunner;

        mutable std::mutex _mutex;
        std::atomic<std::size_t> _ongoingScanCount{};
        std::deque<std::unique_ptr<IFileScanOperation>> _scanResults;
        std::condition_variable _condVar;
        bool& _abort;
    };
} // namespace lms::scanner