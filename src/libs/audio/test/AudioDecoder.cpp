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
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <span>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "audio/Exception.hpp"
#include "audio/IAudioDecoder.hpp"

#include "TestData.hpp"
#include "TestUtils.hpp"

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
        audio,
        AudioDecoderTest,
        ::testing::ValuesIn(getTestAudioFiles()),
        [](const ::testing::TestParamInfo<TestAudioFile>& info) { return std::string{ info.param.name }; });

    namespace
    {
        // Copy of the file with pseudo-random bytes written over [offset, offset + size)
        std::filesystem::path writeDamagedCopy(const ScopedTmpDirectory& directory, const TestAudioFile& file, std::size_t offset, std::size_t size)
        {
            std::vector<std::byte> data(std::filesystem::file_size(file.getPath()));
            std::ifstream is{ file.getPath(), std::ios::binary };
            is.read(static_cast<char*>(static_cast<void*>(data.data())), static_cast<std::streamsize>(data.size()));

            std::minstd_rand randomGenerator;
            for (std::size_t i{ offset }; i < std::min(offset + size, data.size()); ++i)
                data[i] = static_cast<std::byte>(randomGenerator());

            const std::filesystem::path path{ directory / file.fileName };
            writeFile(path, data);
            return path;
        }
    } // namespace

    TEST(AudioDecoder, skipsIsolatedInvalidPackets)
    {
        const std::span<const TestAudioFile> files{ getTestAudioFiles() };
        const auto it{ std::ranges::find(files, "mp3", &TestAudioFile::name) };
        ASSERT_NE(it, files.end());

        const TestAudioFile& file{ *it };
        if (!isDecodingSupported(file.container, file.codec))
            GTEST_SKIP() << "container/codec not supported by this build";

        // The MP3 demuxer resyncs over the damage: only a couple of packets are invalid
        const ScopedTmpDirectory directory;
        const std::filesystem::path path{ writeDamagedCopy(directory, file, 24'000, 1'000) };

        const std::vector<std::int16_t> samples{ decodeInterleaved(path, file.channelCount, file.sampleRate) };

        const double decodedDurationSec{ static_cast<double>(samples.size()) / (file.channelCount * file.sampleRate) };
        EXPECT_NEAR(decodedDurationSec, std::chrono::duration<double>{ file.duration }.count(), 0.2);
    }

    TEST(AudioDecoder, throwsWhenNoPacketIsDecodable)
    {
        const std::span<const TestAudioFile> files{ getTestAudioFiles() };
        const auto it{ std::ranges::find(files, "aac", &TestAudioFile::name) };
        ASSERT_NE(it, files.end());

        const TestAudioFile& file{ *it };
        if (!isDecodingSupported(file.container, file.codec))
            GTEST_SKIP() << "container/codec not supported by this build";

        // The audio data spans bytes [44, 48420), the MP4 index that follows stays intact: every packet is invalid
        const ScopedTmpDirectory directory;
        const std::filesystem::path path{ writeDamagedCopy(directory, file, 44, 48'376) };

        EXPECT_THROW(decodeInterleaved(path, file.channelCount, file.sampleRate), Exception);
    }
} // namespace lms::audio::tests
