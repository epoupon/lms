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

#include <algorithm>
#include <filesystem>
#include <memory>
#include <span>
#include <string>

#include <benchmark/benchmark.h>

#include "audio/IAudioFileInfo.hpp"
#include "audio/IAudioFileInfoParser.hpp"

#include "TestData.hpp"

namespace lms::audio::bench
{
    void registerAudioFileInfoParserBenchmarks()
    {
        for (const AudioFileInfoParserBackend backend : { AudioFileInfoParserBackend::TagLib, AudioFileInfoParserBackend::FFmpeg })
        {
            for (const tests::TestAudioFile& file : tests::getTestAudioFiles())
            {
                const std::filesystem::path path{ file.getPath() };
                const std::string name{ "BM_AudioFileInfoParser/" + std::string{ audioFileInfoParserBackendToString(backend).str() } + "/" + std::string{ file.name } };

                ::benchmark::RegisterBenchmark(name, [backend, path](::benchmark::State& state) {
                    const std::unique_ptr<IAudioFileInfoParser> parser{ createAudioFileInfoParser(backend) };
                    const std::span<const std::filesystem::path> supportedExtensions{ parser->getSupportedExtensions() };
                    if (std::find(std::cbegin(supportedExtensions), std::cend(supportedExtensions), path.extension()) == std::cend(supportedExtensions))
                    {
                        state.SkipWithError("format not supported by this build");
                        return;
                    }

                    for (auto _ : state)
                        ::benchmark::DoNotOptimize(parser->parse(path));
                });
            }
        }
    }
} // namespace lms::audio::bench
