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

#include "FileScanQueue.hpp"

#include <boost/asio/post.hpp>

#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"

#include "scanners/IFileScanOperation.hpp"

namespace lms::scanner
{
    FileScanQueue::FileScanQueue(std::size_t threadCount, bool& abort)
        : _scanContextRunner{ _scanIoContext, threadCount, "FileScan" }
        , _abort{ abort }
    {
    }

    FileScanQueue::~FileScanQueue() = default;

    void FileScanQueue::pushScanRequest(std::unique_ptr<IFileScanOperation> operation)
    {
        {
            std::scoped_lock lock{ _mutex };
            _ongoingScanCount += 1;
        }

        auto operationHandler{ [operation = std::move(operation), this]() mutable {
            if (_abort)
            {
                std::scoped_lock lock{ _mutex };
                _ongoingScanCount -= 1;
            }
            else
            {
                {
                    LMS_SCOPED_TRACE_OVERVIEW("Scanner", operation->getName());
                    LMS_LOG(DBUPDATER, DEBUG, operation->getName() << ": scanning file '" << operation->getFile().string() << "'");
                    operation->scan();
                }

                {
                    std::scoped_lock lock{ _mutex };

                    _scanResults.emplace_back(std::move(operation));
                    _ongoingScanCount -= 1;
                }
            }

            _condVar.notify_all();
        } };

        boost::asio::post(_scanIoContext, std::move(operationHandler));
    }

    std::size_t FileScanQueue::getResultsCount() const
    {
        std::scoped_lock lock{ _mutex };
        return _scanResults.size();
    }

    size_t FileScanQueue::popResults(std::vector<std::unique_ptr<IFileScanOperation>>& results, std::size_t maxCount)
    {
        results.clear();
        results.reserve(maxCount);

        {
            std::scoped_lock lock{ _mutex };

            while (results.size() < maxCount && !_scanResults.empty())
            {
                results.push_back(std::move(_scanResults.front()));
                _scanResults.pop_front();
            }
        }

        return results.size();
    }

    void FileScanQueue::wait(std::size_t maxScanRequestCount)
    {
        if (_ongoingScanCount <= maxScanRequestCount)
            return;

        {
            LMS_SCOPED_TRACE_OVERVIEW("Scanner", "WaitParseResults");

            std::unique_lock lock{ _mutex };
            _condVar.wait(lock, [=, this] { return _ongoingScanCount <= maxScanRequestCount; });
        }
    }

} // namespace lms::scanner
