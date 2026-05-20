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

#include <cstring>
#include <memory>
#include <random>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "audio/IPcmDecoder.hpp"
#include "audio/PcmTypes.hpp"

#include "utils/PcmSpectralFrameDecoder.hpp"

namespace lms::audio::tests
{
    namespace
    {
        // A mock IPcmDecoder that emits samples 0, 1, 2, 3, ... (as float) up to totalSamples,
        class SequencePcmDecoder : public IPcmDecoder
        {
        public:
            SequencePcmDecoder(std::size_t totalSampleCount)
                : _totalSampleCount{ totalSampleCount }
            {
            }

            const PcmParameters& getParameters() const override { return _params; }

            std::size_t readSamples(std::span<WritableBuffer> outputChannelBuffers) override
            {
                assert(outputChannelBuffers.size() == 1); // only planar
                if (_finished)
                    return 0;

                auto& buf{ outputChannelBuffers[0] };
                if (buf.size() == 0)
                    return 0;

                assert(buf.size() % sizeof(float) == 0);

                // not always writing up to what is requested
                std::uniform_int_distribution dist{ std::size_t{ 1 }, buf.size() / sizeof(float) };
                std::size_t sampleCountToWrite{ dist(_randomEngine) };
                if (_currentSampleIndex + sampleCountToWrite > _totalSampleCount)
                {
                    sampleCountToWrite = _totalSampleCount - _currentSampleIndex;
                    _finished = true;
                }

                float* dest{ reinterpret_cast<float*>(buf.data()) };
                for (std::size_t i{}; i < sampleCountToWrite; ++i)
                    *(dest++) = static_cast<float>(_currentSampleIndex++);

                return sampleCountToWrite;
            }

            bool finished() const override { return _finished; }

            std::chrono::milliseconds getEstimatedDuration() const override
            {
                return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<float>{ static_cast<float>(_totalSampleCount) / static_cast<float>(_params.sampleRate) });
            }

        private:
            const PcmParameters _params{
                .channelCount = 1,
                .sampleRate = 16000,
                .sampleType = PcmSampleType::Float32,
                .byteOrder = std::endian::native,
                .planar = false,
            };

            std::minstd_rand _randomEngine{ 42 }; // fixed seed for reproducibility
            std::size_t _totalSampleCount{};
            std::size_t _currentSampleIndex{};
            bool _finished{};
        };

        template<typename T, std::size_t N>
        void expectSpanEq(std::span<const T, N> actual, const std::array<T, N>& expected)
        {
            for (std::size_t i{}; i < N; ++i)
                EXPECT_FLOAT_EQ(actual[i], expected[i]) << "index=" << i;
        }

        constexpr std::size_t WindowSize{ 8 };
        constexpr std::size_t HopSize{ 4 };
        using FrameDecoder = PcmSpectralFrameDecoder<WindowSize, float>;
    } // namespace

    TEST(SequencePcmDecoder, basic)
    {
        constexpr std::size_t decoderTotalSampleCount{ 32 };
        SequencePcmDecoder decoder{ decoderTotalSampleCount };

        std::size_t totalSampleReadCount{};
        while (true)
        {
            std::array<float, 16> buffer{};
            std::array outputBuffers{ IPcmDecoder::WritableBuffer{ std::as_writable_bytes(std::span{ buffer }) } };
            const std::size_t sampleReadCount{ decoder.readSamples(outputBuffers) };
            if (sampleReadCount == 0)
                break;

            for (std::size_t i{}; i < sampleReadCount; ++i)
                EXPECT_FLOAT_EQ(buffer[i], totalSampleReadCount + i);

            totalSampleReadCount += sampleReadCount;
        }

        EXPECT_EQ(totalSampleReadCount, decoderTotalSampleCount);
    }

    TEST(PcmSpectralFrameDecoder, firstFrameIsCenteredOnSample0)
    {
        FrameDecoder frameDecoder{ std::make_unique<SequencePcmDecoder>(32), HopSize };
        using Frame = std::array<float, WindowSize>;
        std::vector<Frame> frames;

        EXPECT_EQ(frameDecoder.currentFrameIndex(), 0);
        const std::size_t decoded{ frameDecoder.decodeFrames(1,
                                                             [&](const FrameDecoder::SpectralFrameView& frame) {
                                                                 auto& newFrame{ frames.emplace_back() };
                                                                 std::copy(std::cbegin(frame.rawSamples), std::cend(frame.rawSamples), std::begin(newFrame));
                                                             }) };

        ASSERT_EQ(decoded, 1);
        ASSERT_EQ(frames.size(), 1);
        EXPECT_EQ(frameDecoder.currentFrameIndex(), 1);
        expectSpanEq<float, WindowSize>(frames[0], { 0.F, 0.F, 0.F, 0.F, 0.F, 1.F, 2.F, 3.F });
    }

    TEST(PcmSpectralFrameDecoder, framesAdvanceByHopSize)
    {
        FrameDecoder frameDecoder{ std::make_unique<SequencePcmDecoder>(16), HopSize };
        using Frame = std::array<float, WindowSize>;
        std::vector<Frame> frames;

        const std::size_t decoded{ frameDecoder.decodeFrames(2,
                                                             [&](const FrameDecoder::SpectralFrameView& frame) {
                                                                 auto& newFrame{ frames.emplace_back() };
                                                                 std::copy(std::cbegin(frame.rawSamples), std::cend(frame.rawSamples), std::begin(newFrame));
                                                             }) };

        ASSERT_EQ(decoded, 2);
        ASSERT_EQ(frames.size(), 2);
        EXPECT_EQ(frameDecoder.currentFrameIndex(), 2);

        expectSpanEq<float, WindowSize>(
            frames[0],
            { 0.F, 0.F, 0.F, 0.F, 0.F, 1.F, 2.F, 3.F });

        expectSpanEq<float, WindowSize>(
            frames[1],
            { 0.F, 1.F, 2.F, 3.F, 4.F, 5.F, 6.F, 7.F });
    }

    TEST(PcmSpectralFrameDecoder, skipFramesAdvancesState)
    {
        FrameDecoder frameDecoder{ std::make_unique<SequencePcmDecoder>(32), HopSize };
        using Frame = std::array<float, WindowSize>;
        std::vector<Frame> frames;

        EXPECT_EQ(frameDecoder.currentFrameIndex(), 0);
        ASSERT_EQ(frameDecoder.skipFrames(2), 2);
        EXPECT_EQ(frameDecoder.currentFrameIndex(), 2);

        Frame lastFrame{};

        ASSERT_EQ(frameDecoder.decodeFrames(1,
                                            [&](const FrameDecoder::SpectralFrameView& frame) {
                                                std::copy(std::cbegin(frame.rawSamples), std::cend(frame.rawSamples), std::begin(lastFrame));
                                            }),
                  1);

        expectSpanEq<float, WindowSize>(
            lastFrame,
            { 4.F, 5.F, 6.F, 7.F, 8.F, 9.F, 10.F, 11.F });

        EXPECT_EQ(frameDecoder.currentFrameIndex(), 3);
    }
} // namespace lms::audio::tests
