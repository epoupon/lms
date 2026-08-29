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

#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "audio/IAudioDecoder.hpp"

#include "TestData.hpp"

namespace lms::audio::tests
{
    class AudioDecoderTest : public ::testing::TestWithParam<TestAudioFile>
    {
    };

    TEST_P(AudioDecoderTest, decodesToExpectedDuration)
    {
        const TestAudioFile& testCase{ GetParam() };
        const std::filesystem::path path{ testCase.getPath() };
        ASSERT_TRUE(std::filesystem::exists(path)) << path;

        if (!isDecodingSupported(testCase.container, testCase.codec))
            GTEST_SKIP() << "container/codec not supported by this build";

        const PcmParameters params{
            .channelCount = testCase.channelCount,
            .sampleRate = testCase.sampleRate,
            .sampleType = PcmSampleType::Signed16,
            .byteOrder = std::endian::native,
            .planar = false,
        };

        const std::unique_ptr<IAudioDecoder> decoder{ createAudioDecoder(path, std::chrono::microseconds{ 0 }, params) };

        constexpr std::size_t bufferSampleCount{ 8'192 };
        std::vector<std::byte> buffer(bufferSampleCount * testCase.channelCount * sizeof(std::int16_t));
        std::array outputBuffers{ IAudioDecoder::WritableBuffer{ std::span{ buffer } } };

        std::size_t totalSampleCount{};
        while (!decoder->finished())
        {
            const std::size_t sampleCount{ decoder->readSamples(outputBuffers) };
            if (sampleCount == 0)
                break;

            totalSampleCount += sampleCount;
        }

        ASSERT_GT(totalSampleCount, 0U);

        const double decodedDurationSec{ static_cast<double>(totalSampleCount) / testCase.sampleRate };
        EXPECT_NEAR(decodedDurationSec, std::chrono::duration<double>{ testCase.duration }.count(), 0.2);
    }

    INSTANTIATE_TEST_SUITE_P(
        Formats,
        AudioDecoderTest,
        ::testing::ValuesIn(getTestAudioFiles()),
        [](const ::testing::TestParamInfo<TestAudioFile>& info) { return std::string{ info.param.name }; });
} // namespace lms::audio::tests
