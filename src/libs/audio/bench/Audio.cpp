/*
 * Copyright (C) 2026 Emeric Poupon
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

#include <cstdlib>

#include <benchmark/benchmark.h>

#include "core/ILogger.hpp"
#include "core/Service.hpp"

#include "audio/Init.hpp"

namespace lms::audio::bench
{
    void registerAudioFileInfoParserBenchmarks();
} // namespace lms::audio::bench

int main(int argc, char** argv)
{
    lms::core::Service<lms::core::logging::ILogger> logger{ lms::core::logging::createLogger(lms::core::logging::Severity::ERROR) };
    lms::audio::init();

    lms::audio::bench::registerAudioFileInfoParserBenchmarks();

    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv))
        return EXIT_FAILURE;

    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();

    return EXIT_SUCCESS;
}
