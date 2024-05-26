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

#include <sstream>
#include <thread>

#include <gtest/gtest.h>

#include "core/ITraceLogger.hpp"

namespace lms::core::tracing::tests
{
    // not much can be tested with this implementation
    TEST(TraceLogger, MultipleThreads)
    {
        auto traceLogger{ createTraceLogger(Level::Overview) };

        std::vector<std::thread> threads;
        for (std::size_t i{}; i < 16; ++i)
        {
            threads.emplace_back([&] {
                ScopedTrace loggedEvent{ "MyCategory", Level::Overview, "MyEventLogged", "SomeArgType", "SomeArg", traceLogger.get() };
                ScopedTrace notLoggedEvent{ "MyNotLoggedCategory", Level::Detailed, "MyEventNotLogged", "SomeNotLoggedArgType", "SomeNotLoggedArg", traceLogger.get() };
            });
        }

        for (std::thread& t : threads)
            t.join();

        std::ostringstream oss;
        traceLogger->dumpCurrentBuffer(oss);

        EXPECT_NE(oss.str().find("MyEventLogged"), std::string::npos);
        EXPECT_NE(oss.str().find("MyCategory"), std::string::npos);
        EXPECT_NE(oss.str().find("SomeArgType"), std::string::npos);
        EXPECT_NE(oss.str().find("SomeArg"), std::string::npos);

        EXPECT_EQ(oss.str().find("MyEventNotLogged"), std::string::npos);
        EXPECT_EQ(oss.str().find("MyNotLoggedCategory"), std::string::npos);
        EXPECT_EQ(oss.str().find("SomeNotLoggedArgType"), std::string::npos);
        EXPECT_EQ(oss.str().find("SomeNotLoggedArg"), std::string::npos);
    }
} // namespace lms::core::tracing::tests