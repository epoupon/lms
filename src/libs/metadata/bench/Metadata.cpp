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

#include <benchmark/benchmark.h>

#include "AudioFileParser.hpp"
#include "TestTagReader.hpp"
#include "core/String.hpp"
#include "metadata/Types.hpp"

namespace lms::metadata::benchmarks
{
    class TestAudioFileParser : public AudioFileParser
    {
    public:
        using AudioFileParser::AudioFileParser;
        using AudioFileParser::parseMetaData;
    };

    static void BM_Metadata_parse(benchmark::State& state)
    {
        AudioFileParserParameters params;
        params.userExtraTags = { "MY_AWESOME_TAG_A", "MY_AWESOME_TAG_B", "MY_AWESOME_MISSING_TAG" };

        std::unique_ptr<ITagReader> testTags{ tests::createDefaultPopulatedTestTagReader() };
        const TestAudioFileParser parser{ params };
        for (auto _ : state)
        {
            std::unique_ptr<Track> track{ parser.parseMetaData(*testTags) };
        }
    }

    static void BM_Metadata_parseArtists(benchmark::State& state)
    {
        const tests::TestTagReader testTags{
            {
                { TagType::Artist, { "AC/DC; MyArtist" } },
            }
        };

        const AudioFileParserParameters params;
        const TestAudioFileParser parser{ params };

        for (auto _ : state)
        {
            std::unique_ptr<Track> track{ parser.parseMetaData(testTags) };
        }
    }

    static void BM_Metadata_parseArtists_WithWhitelist(benchmark::State& state)
    {
        const tests::TestTagReader testTags{
            {
                { TagType::Artist, { "AC/DC; MyArtist" } },
            }
        };

        AudioFileParserParameters params;
        params.artistTagDelimiters = { "/", ";" };
        // The list itself is not important, the idea is to have some volume
        params.artistsToNotSplit = { "AC/DC",
            "+/-",
            R"(A/N【eɪ-ɛn)",
            "Akron/Family",
            "AM/FM",
            "Ashes/Dust",
            "B/B/S/",
            "BLCK/MRKT/RGNS",
            "Body/Gate/Head",
            "Body/Head",
            "Born/Dead",
            "Burger/Ink",
            "case/lang/veirs",
            "Chicago / London Underground",
            "Dakota/Dakota",
            "Dark/Light",
            "Decades/Failures",
            "The Denison/Kimball Trio",
            "D-W/L-SS",
            "F/i",
            "Friend / Enemy",
            "GZA/Genius",
            "I/O",
            "I/O3",
            "In/Humanity",
            "Love/Lust",
            "Mirror/Dash",
            "Model/Actress",
            "N/N",
            "Neither/Neither World",
            "P1/E",
            "Sick/Tired",
            "t/e/u/",
            "tide/edit",
            "V/Vm",
            "White/Lichens",
            "White/Light",
            "Yamantaka // Sonic Titan" };

        const TestAudioFileParser parser{ params };
        for (auto _ : state)
        {
            std::unique_ptr<Track> track{ parser.parseMetaData(testTags) };
        }
    }

    static void BM_Metadata_parseArtists_WithoutWhitelist(benchmark::State& state)
    {
        const tests::TestTagReader testTags{
            {
                { TagType::Artist, { "AC/DC; MyArtist" } },
            }
        };

        AudioFileParserParameters params;
        params.artistTagDelimiters = { "/", ";" };

        const TestAudioFileParser parser{ params };

        for (auto _ : state)
        {
            std::unique_ptr<Track> track{ parser.parseMetaData(testTags) };
        }
    }

    BENCHMARK(BM_Metadata_parse);
    BENCHMARK(BM_Metadata_parseArtists);
    BENCHMARK(BM_Metadata_parseArtists_WithWhitelist);
    BENCHMARK(BM_Metadata_parseArtists_WithoutWhitelist);

} // namespace lms::metadata::benchmarks

BENCHMARK_MAIN();