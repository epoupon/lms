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

#include <chrono>
#include <filesystem>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "audio/AudioProperties.hpp"
#include "audio/IAudioFileInfo.hpp"
#include "audio/IAudioFileInfoParser.hpp"
#include "audio/IImageReader.hpp"
#include "audio/ITagReader.hpp"

#include "TestData.hpp"

namespace lms::audio::tests
{
    namespace
    {
        struct ParserTestCase
        {
            std::string name; // must be a valid gtest identifier
            AudioFileInfoParserBackend backend;
            TestAudioFile file;
        };

        std::ostream& operator<<(std::ostream& os, const ParserTestCase& testCase)
        {
            return os << testCase.name;
        }

        std::vector<ParserTestCase> buildParserTestCases()
        {
            std::vector<ParserTestCase> cases;
            for (const AudioFileInfoParserBackend backend : { AudioFileInfoParserBackend::TagLib, AudioFileInfoParserBackend::FFmpeg })
            {
                for (const TestAudioFile& file : getTestAudioFiles())
                    cases.push_back(ParserTestCase{ std::string{ audioFileInfoParserBackendToString(backend).str() } + "_" + std::string{ file.name }, backend, file });
            }
            return cases;
        }

        std::string getFirstTagValue(const ITagReader& tagReader, TagType type)
        {
            std::string result;
            tagReader.visitTagValues(type, [&](std::string_view value) {
                if (result.empty())
                    result = std::string{ value };
            });
            return result;
        }
    } // namespace

    class AudioFileInfoParser : public ::testing::TestWithParam<ParserTestCase>
    {
    };

    TEST_P(AudioFileInfoParser, parse)
    {
        const ParserTestCase& testCase{ GetParam() };
        const std::filesystem::path path{ testCase.file.getPath() };
        ASSERT_TRUE(std::filesystem::exists(path)) << path;

        const std::unique_ptr<IAudioFileInfoParser> parser{ createAudioFileInfoParser(testCase.backend) };
        const std::unique_ptr<IAudioFileInfo> info{ parser->parse(path) };
        ASSERT_TRUE(info);

        const AudioProperties* properties{ info->getAudioProperties() };
        ASSERT_TRUE(properties);

        EXPECT_EQ(properties->container, testCase.file.container);
        EXPECT_EQ(properties->codec, testCase.file.codec);
        EXPECT_EQ(properties->channelCount, testCase.file.channelCount);
        EXPECT_EQ(properties->sampleRate, testCase.file.sampleRate);
        EXPECT_NEAR(std::chrono::duration<double>{ properties->duration }.count(), std::chrono::duration<double>{ testCase.file.duration }.count(), 0.2);

        const ITagReader* tagReader{ info->getTagReader() };
        ASSERT_TRUE(tagReader);

        EXPECT_EQ(getFirstTagValue(*tagReader, TagType::TrackTitle), testTagTitle.str());
        EXPECT_EQ(getFirstTagValue(*tagReader, TagType::Artist), testTagArtist.str());
        EXPECT_EQ(getFirstTagValue(*tagReader, TagType::Album), testTagAlbum.str());

        if (testCase.file.hasCoverArt)
        {
            const IImageReader* imageReader{ info->getImageReader() };
            ASSERT_TRUE(imageReader);

            std::size_t imageCount{};
            imageReader->visitImages([&](const Image&) { ++imageCount; });
            EXPECT_GT(imageCount, 0U);
        }
    }

    INSTANTIATE_TEST_SUITE_P(
        audio,
        AudioFileInfoParser,
        ::testing::ValuesIn(buildParserTestCases()),
        [](const ::testing::TestParamInfo<ParserTestCase>& info) { return info.param.name; });
} // namespace lms::audio::tests
