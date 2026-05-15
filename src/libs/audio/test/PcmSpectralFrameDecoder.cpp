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
        // returning them in chunks of at most chunkSize per call, with a randomly varying chunk
        // size (uniform in [1, chunkSize]) to stress the partial-read handling in readAtLeastSamples.
        class SequencePcmDecoder : public IPcmDecoder
        {
        public:
            SequencePcmDecoder(std::size_t totalSamples, std::size_t chunkSize)
                : _totalSamples{ totalSamples }
                , _maxChunkSize{ chunkSize }
                , _chunkDist{ 1, chunkSize }
            {
            }

            const PcmParameters& getParameters() const override { return _params; }

            std::size_t readSamples(std::span<WritableBuffer> outputChannelBuffers) override
            {
                if (_finished || outputChannelBuffers.empty())
                    return 0;

                auto& buf{ outputChannelBuffers[0] };
                const std::size_t maxSamples{ buf.size() / sizeof(float) };
                const std::size_t remaining{ _totalSamples - _nextSample };
                const std::size_t chunkSize{ _chunkDist(_rng) };
                const std::size_t toWrite{ std::min({ maxSamples, chunkSize, remaining }) };

                auto* dest{ reinterpret_cast<float*>(buf.data()) };
                for (std::size_t i{}; i < toWrite; ++i)
                    dest[i] = static_cast<float>(_nextSample + i);

                _nextSample += toWrite;
                if (_nextSample >= _totalSamples)
                    _finished = true;

                return toWrite;
            }

            bool finished() const override { return _finished; }

        private:
            const PcmParameters _params{
                .channelCount = 1,
                .sampleRate = 16000,
                .sampleType = PcmSampleType::Float32,
                .byteOrder = std::endian::native,
                .planar = false,
            };

            const std::size_t _totalSamples;
            const std::size_t _maxChunkSize;
            std::minstd_rand _rng{ 42 }; // fixed seed for reproducibility
            std::uniform_int_distribution<std::size_t> _chunkDist;
            std::size_t _nextSample{};
            bool _finished{};
        };

        constexpr std::size_t WindowSize{ 8 };
        constexpr std::size_t HopSize{ 4 };

        using FrameDecoder = PcmSpectralFrameDecoder<WindowSize, float>;

        // Returns the sample at `paddedIndex` in the zero-padded sequence for a given windowSize.
        float expectedSample(std::ptrdiff_t paddedIndex, std::size_t windowSize)
        {
            const std::ptrdiff_t centerPad{ static_cast<std::ptrdiff_t>(windowSize / 2) };
            return paddedIndex < centerPad ? 0.F : static_cast<float>(paddedIndex - centerPad);
        }

        // For frame N (0-based), its raw window covers padded-sequence indices [N*hop, N*hop + windowSize).
        std::vector<float> expectedRawSamples(std::size_t frameIndex, std::size_t hopSize, std::size_t windowSize)
        {
            std::vector<float> samples(windowSize);
            const auto start{ static_cast<std::ptrdiff_t>(frameIndex * hopSize) };
            for (std::size_t i{}; i < windowSize; ++i)
                samples[i] = expectedSample(start + static_cast<std::ptrdiff_t>(i), windowSize);
            return samples;
        }
    } // namespace

    // Verify that a single decodeFrames(N) call delivers exactly N frames whose raw samples
    // match the expected window positions — no sample is skipped or duplicated.
    TEST(PcmSpectralFrameDecoder, singleBatchSampleAlignment)
    {
        constexpr std::size_t totalSamples{ 200 };
        constexpr std::size_t chunkSize{ 32 }; // deliberately smaller than the batch requirement
        constexpr std::size_t framesToDecode{ 5 };

        auto decoder{ std::make_unique<SequencePcmDecoder>(totalSamples, chunkSize) };
        FrameDecoder frameDecoder{ std::move(decoder), HopSize };

        std::size_t frameIdx{};
        const auto callback{ [&](const FrameDecoder::SpectralFrameView& frame) {
            const std::vector<float> expected{ expectedRawSamples(frameIdx, HopSize, WindowSize) };
            for (std::size_t i{}; i < WindowSize; ++i)
                EXPECT_FLOAT_EQ(frame.rawSamples[i], expected[i]) << "frame " << frameIdx << " sample " << i;
            ++frameIdx;
        } };

        const std::size_t decoded{ frameDecoder.decodeFrames(framesToDecode, callback) };
        EXPECT_EQ(decoded, framesToDecode);
        EXPECT_EQ(frameIdx, framesToDecode);
    }

    // Verify that consecutive decodeFrames calls maintain correct continuity:
    // the window for frame N always starts exactly at sample N*hop in the padded sequence.
    TEST(PcmSpectralFrameDecoder, consecutiveBatchContinuity)
    {
        constexpr std::size_t totalSamples{ 500 };
        constexpr std::size_t chunkSize{ 17 }; // odd chunk to stress the fill loop
        constexpr std::size_t framesPerBatch{ 3 };

        auto decoder{ std::make_unique<SequencePcmDecoder>(totalSamples, chunkSize) };
        FrameDecoder frameDecoder{ std::move(decoder), HopSize };

        std::size_t globalFrameIdx{};
        bool continueDecoding{ true };

        while (continueDecoding)
        {
            std::size_t batchFrameIdx{ globalFrameIdx };
            const auto callback{ [&](const FrameDecoder::SpectralFrameView& frame) {
                const std::vector<float> expected{ expectedRawSamples(batchFrameIdx, HopSize, WindowSize) };
                for (std::size_t i{}; i < WindowSize; ++i)
                    EXPECT_FLOAT_EQ(frame.rawSamples[i], expected[i]) << "frame " << batchFrameIdx << " sample " << i;
                ++batchFrameIdx;
            } };

            const std::size_t decoded{ frameDecoder.decodeFrames(framesPerBatch, callback) };
            globalFrameIdx = batchFrameIdx;
            if (decoded < framesPerBatch)
                continueDecoding = false;
        }

        // Must have decoded at least some frames
        EXPECT_GT(globalFrameIdx, 0U);
    }

    // Verify that skipFrames advances the position correctly so the next decodeFrames
    // sees the right window.
    TEST(PcmSpectralFrameDecoder, skipFramesAlignment)
    {
        constexpr std::size_t totalSamples{ 500 };
        constexpr std::size_t chunkSize{ 64 };
        constexpr std::size_t skipCount{ 3 };
        constexpr std::size_t decodeCount{ 2 };

        auto decoder{ std::make_unique<SequencePcmDecoder>(totalSamples, chunkSize) };
        FrameDecoder frameDecoder{ std::move(decoder), HopSize };

        // Skip 3 frames
        const std::size_t skipped{ frameDecoder.skipFrames(skipCount) };
        EXPECT_EQ(skipped, skipCount);
        EXPECT_EQ(frameDecoder.currentFrameIndex(), skipCount);

        // Decode 2 frames — they should correspond to frames [3, 4] in the global sequence
        std::size_t frameIdx{ skipCount };
        const auto callback{ [&](const FrameDecoder::SpectralFrameView& frame) {
            const std::vector<float> expected{ expectedRawSamples(frameIdx, HopSize, WindowSize) };
            for (std::size_t i{}; i < WindowSize; ++i)
                EXPECT_FLOAT_EQ(frame.rawSamples[i], expected[i]) << "frame " << frameIdx << " sample " << i;
            ++frameIdx;
        } };

        const std::size_t decoded{ frameDecoder.decodeFrames(decodeCount, callback) };
        EXPECT_EQ(decoded, decodeCount);
        EXPECT_EQ(frameDecoder.currentFrameIndex(), skipCount + decodeCount);
    }

    // Verify that decodeFrames returns a partial count at EOF, and 0 only when no frame can be decoded.
    TEST(PcmSpectralFrameDecoder, eofReturnsPartialCount)
    {
        // center_pad=4, totalSamples=11 → 15 buffered samples total.
        // Frame 0: [0..7] → OK, consume 4 → 11 left.
        // Frame 1: [0..7] → OK, consume 4 → 7 left.
        // Frame 2: needs 8, only 7 available → stop.
        // decodeFrames(3) returns 2; subsequent call returns 0.
        constexpr std::size_t totalSamples{ 11 };
        constexpr std::size_t chunkSize{ 64 };

        auto decoder{ std::make_unique<SequencePcmDecoder>(totalSamples, chunkSize) };
        FrameDecoder frameDecoder{ std::move(decoder), HopSize };

        std::size_t callbackCount{};
        const auto callback{ [&](const FrameDecoder::SpectralFrameView&) { ++callbackCount; } };

        const std::size_t decoded{ frameDecoder.decodeFrames(3, callback) };
        EXPECT_EQ(decoded, 2U);
        EXPECT_EQ(callbackCount, 2U);

        const std::size_t decoded2{ frameDecoder.decodeFrames(3, callback) };
        EXPECT_EQ(decoded2, 0U);
        EXPECT_EQ(callbackCount, 2U);
    }

    // Verify that hop == window (non-overlapping) produces correct window positions.
    TEST(PcmSpectralFrameDecoder, hopSizeEqualsWindowSize)
    {
        constexpr std::size_t WS{ 8 };
        constexpr std::size_t HS{ 8 };
        using Decoder = PcmSpectralFrameDecoder<WS, float>;

        constexpr std::size_t totalSamples{ 200 };
        constexpr std::size_t framesToDecode{ 5 };

        auto decoder{ std::make_unique<SequencePcmDecoder>(totalSamples, 32) };
        Decoder frameDecoder{ std::move(decoder), HS };

        std::size_t frameIdx{};
        const auto callback{ [&](const Decoder::SpectralFrameView& frame) {
            const std::vector<float> expected{ expectedRawSamples(frameIdx, HS, WS) };
            for (std::size_t i{}; i < WS; ++i)
                EXPECT_FLOAT_EQ(frame.rawSamples[i], expected[i]) << "frame " << frameIdx << " sample " << i;
            ++frameIdx;
        } };

        EXPECT_EQ(frameDecoder.decodeFrames(framesToDecode, callback), framesToDecode);
        EXPECT_EQ(frameIdx, framesToDecode);
    }

    // Verify that hop > window (gaps between frames) produces correct window positions.
    // consumeSamples must discard more than WindowSize per frame.
    TEST(PcmSpectralFrameDecoder, hopSizeGreaterThanWindowSize)
    {
        constexpr std::size_t WS{ 8 };
        constexpr std::size_t HS{ 16 };
        using Decoder = PcmSpectralFrameDecoder<WS, float>;

        constexpr std::size_t totalSamples{ 400 };
        constexpr std::size_t framesToDecode{ 5 };

        auto decoder{ std::make_unique<SequencePcmDecoder>(totalSamples, 32) };
        Decoder frameDecoder{ std::move(decoder), HS };

        std::size_t frameIdx{};
        const auto callback{ [&](const Decoder::SpectralFrameView& frame) {
            const std::vector<float> expected{ expectedRawSamples(frameIdx, HS, WS) };
            for (std::size_t i{}; i < WS; ++i)
                EXPECT_FLOAT_EQ(frame.rawSamples[i], expected[i]) << "frame " << frameIdx << " sample " << i;
            ++frameIdx;
        } };

        EXPECT_EQ(frameDecoder.decodeFrames(framesToDecode, callback), framesToDecode);
        EXPECT_EQ(frameIdx, framesToDecode);
    }

    // Verify skipFrames returns 0 when EOF is reached before the required hop samples are available.
    TEST(PcmSpectralFrameDecoder, skipFramesReturnsZeroAtEof)
    {
        // center_pad=4, totalSamples=3 → 7 buffered samples.
        // skipFrames(1) needs 4 ≤ 7 → succeeds. Leaves 3 buffered.
        // skipFrames(1) needs 4 > 3 → returns 0.
        constexpr std::size_t totalSamples{ 3 };

        auto decoder{ std::make_unique<SequencePcmDecoder>(totalSamples, 64) };
        FrameDecoder frameDecoder{ std::move(decoder), HopSize };

        EXPECT_EQ(frameDecoder.skipFrames(1), 1U);
        EXPECT_EQ(frameDecoder.skipFrames(1), 0U);
        EXPECT_EQ(frameDecoder.currentFrameIndex(), 1U);
    }

    // Verify decodeFrames(0) is a no-op.
    TEST(PcmSpectralFrameDecoder, decodeFramesZeroCount)
    {
        auto decoder{ std::make_unique<SequencePcmDecoder>(200, 64) };
        FrameDecoder frameDecoder{ std::move(decoder), HopSize };

        std::size_t callbackCount{};
        const std::size_t decoded{ frameDecoder.decodeFrames(0, [&](const FrameDecoder::SpectralFrameView&) { ++callbackCount; }) };
        EXPECT_EQ(decoded, 0U);
        EXPECT_EQ(callbackCount, 0U);
        EXPECT_EQ(frameDecoder.currentFrameIndex(), 0U);
    }

    // Verify totalDecodedSamples tracks real file samples only (not center-padding zeros).
    TEST(PcmSpectralFrameDecoder, totalDecodedSamplesExcludesPadding)
    {
        constexpr std::size_t totalSamples{ 200 };
        constexpr std::size_t chunkSize{ 64 };

        auto decoder{ std::make_unique<SequencePcmDecoder>(totalSamples, chunkSize) };
        FrameDecoder frameDecoder{ std::move(decoder), HopSize };

        // Drain all frames
        const auto noop{ [](const FrameDecoder::SpectralFrameView&) {} };
        while (frameDecoder.decodeFrames(1, noop) != 0)
        {
        }

        EXPECT_EQ(frameDecoder.totalDecodedSamples(), totalSamples);
    }
} // namespace lms::audio::tests
