/*
 * Copyright (C) 2021 Emeric Poupon
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

#include "core/IOContextRunner.hpp"

#include <cstdlib>

#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"

namespace lms::core
{
    IOContextRunner::IOContextRunner(boost::asio::io_context& ioContext, std::size_t threadCount, std::string_view name)
        : _ioContext{ ioContext }
        , _work{ boost::asio::make_work_guard(ioContext) }
    {
        LMS_LOG(UTILS, INFO, "Starting IO context with " << threadCount << " threads...");

        for (std::size_t i{}; i < threadCount; ++i)
        {
            std::string threadName{ name };
            if (!threadName.empty())
            {
                threadName += "Thread_";
                threadName += std::to_string(i);
            }

            _threads.emplace_back([this, threadName] {
                if (!threadName.empty())
                {
                    if (auto* traceLogger{ Service<tracing::ITraceLogger>::get() })
                        traceLogger->setThreadName(std::this_thread::get_id(), threadName);
                }

                try
                {
                    _ioContext.run();
                }
                catch (const std::exception& e)
                {
                    LMS_LOG(UTILS, FATAL, "Exception caught in IO context: " << e.what());
                    std::abort();
                }
            });
        }
    }

    void IOContextRunner::stop()
    {
        LMS_LOG(UTILS, DEBUG, "Stopping IO context...");
        _work.reset();
        _ioContext.stop();
        LMS_LOG(UTILS, DEBUG, "IO context stopped!");
    }

    std::size_t IOContextRunner::getThreadCount() const
    {
        return _threads.size();
    }

    IOContextRunner::~IOContextRunner()
    {
        stop();

        for (std::thread& t : _threads)
            t.join();
    }
} // namespace lms::core