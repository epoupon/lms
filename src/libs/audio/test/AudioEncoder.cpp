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
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <numbers>
#include <ostream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "core/media/Codec.hpp"
#include "core/media/ContainerCodec.hpp"

#include "audio/IAudioDecoder.hpp"
#include "audio/PcmTypes.hpp"

#include "ffmpeg/AudioEncoder.hpp"
#include "ffmpeg/Utils.hpp"

#include "TestUtils.hpp"

namespace lms::audio::tests
{
    namespace
    {
        constexpr std::chrono::milliseconds defaultDuration{ 3'000 };

        // Generates a sine wave directly in whatever PCM layout the encoder negotiated
        struct GeneratedSamples
        {
            std::vector<std::vector<std::byte>> channelBuffers; // 1 buffer if interleaved, one per channel if planar
            std::size_t sampleCountPerChannel{};
        };

        template<typename T>
        void writeSample(std::vector<std::byte>& buffer, T value)
        {
            std::array<std::byte, sizeof(T)> bytes;
            std::memcpy(bytes.data(), &value, sizeof(T));
            buffer.insert(std::end(buffer), std::cbegin(bytes), std::cend(bytes));
        }

        GeneratedSamples generateSineWave(const PcmParameters& params, std::chrono::milliseconds duration)
        {
            GeneratedSamples res;
            res.sampleCountPerChannel = helpers::durationToSampleCount(duration, params.sampleRate);
            res.channelBuffers.resize(params.planar ? params.channelCount : 1);

            for (std::size_t frame{}; frame < res.sampleCountPerChannel; ++frame)
            {
                for (unsigned channel{}; channel < params.channelCount; ++channel)
                {
                    const double phase{ 2 * std::numbers::pi * 440.0 * (channel + 1) * frame / params.sampleRate };
                    const double normalized{ std::sin(phase) };

                    std::vector<std::byte>& buffer{ res.channelBuffers[params.planar ? channel : 0] };
                    switch (params.sampleType)
                    {
                    case PcmSampleType::Signed16:
                        writeSample<std::int16_t>(buffer, static_cast<std::int16_t>(std::lround(normalized * 16'000)));
                        break;
                    case PcmSampleType::Signed32:
                        writeSample<std::int32_t>(buffer, static_cast<std::int32_t>(std::lround(normalized * 16'000 * 65'536)));
                        break;
                    case PcmSampleType::Float32:
                        writeSample<float>(buffer, static_cast<float>(normalized * 0.5));
                        break;
                    case PcmSampleType::Float64:
                        writeSample<double>(buffer, normalized * 0.5);
                        break;
                    }
                }
            }

            return res;
        }

        // Only used for lossless round trips, which are always requested/verified as 16-bit PCM
        std::vector<std::int16_t> toInterleavedInt16(const GeneratedSamples& samples, unsigned channelCount, bool planar)
        {
            std::vector<std::int16_t> res(samples.sampleCountPerChannel * channelCount);

            if (!planar)
            {
                std::memcpy(res.data(), samples.channelBuffers.at(0).data(), res.size() * sizeof(std::int16_t));
                return res;
            }

            for (unsigned channel{}; channel < channelCount; ++channel)
            {
                const auto* src{ static_cast<const std::int16_t*>(static_cast<const void*>(samples.channelBuffers.at(channel).data())) };
                for (std::size_t frame{}; frame < samples.sampleCountPerChannel; ++frame)
                    res[frame * channelCount + channel] = src[frame];
            }

            return res;
        }

        std::vector<std::byte> encode(ffmpeg::AudioEncoder& encoder, const GeneratedSamples& samples)
        {
            std::vector<ffmpeg::AudioEncoder::ReadableBuffer> views;
            for (const std::vector<std::byte>& buffer : samples.channelBuffers)
                views.push_back(std::span{ buffer });

            encoder.writeSamples(views, samples.sampleCountPerChannel);
            encoder.flush();

            std::vector<std::byte> res;
            std::array<std::byte, 65'536> buffer{};
            while (!encoder.finished())
            {
                const std::size_t byteCount{ encoder.readBytes(buffer) };
                if (byteCount == 0)
                    break;

                res.insert(std::end(res), std::cbegin(buffer), std::cbegin(buffer) + byteCount);
            }

            return res;
        }

        ffmpeg::EncodeParameters createParameters(core::media::Container container, core::media::Codec codec, unsigned channelCount = 2, unsigned sampleRate = 44'100)
        {
            return ffmpeg::EncodeParameters{
                .container = container,
                .codec = codec,
                .bitrate = core::media::getCodecDesc(codec).isLossless ? std::optional<unsigned>{} : std::optional<unsigned>{ 128'000 },
                .bitsPerSample = 16,
                .channelCount = channelCount,
                .sampleRate = sampleRate,
                .metadata = {},
            };
        }

        struct RoundTripTestCase
        {
            std::string name; // valid gtest identifier, also used as the output file extension
            core::media::Container container;
            core::media::Codec codec;
        };

        std::ostream& operator<<(std::ostream& os, const RoundTripTestCase& testCase)
        {
            return os << testCase.name;
        }

        // gtest requires param names to be valid C++ identifiers (e.g. codec name "E-AC3" is not)
        std::string sanitizeIdentifier(std::string_view name)
        {
            std::string res{ name };
            std::replace_if(res.begin(), res.end(), [](char c) { return !std::isalnum(static_cast<unsigned char>(c)); }, '_');
            return res;
        }

        std::vector<RoundTripTestCase> buildRoundTripTestCases()
        {
            std::vector<RoundTripTestCase> cases;

            core::media::visitContainerCodecPairs([&](const core::media::ContainerCodec& pair) {
                cases.push_back(RoundTripTestCase{
                    .name = sanitizeIdentifier(core::media::containerToString(pair.container).str()) + "_" + sanitizeIdentifier(core::media::getCodecDesc(pair.codec).name.str()),
                    .container = pair.container,
                    .codec = pair.codec,
                });
            });

            return cases;
        }
    } // namespace

    class AudioEncoder : public ::testing::TestWithParam<RoundTripTestCase>
    {
    };

    TEST_P(AudioEncoder, roundTrips)
    {
        const RoundTripTestCase& testCase{ GetParam() };

        if (!ffmpeg::utils::isCodecMuxingSupported(testCase.container, testCase.codec))
        {
            GTEST_SKIP() << "container/codec combination not supported by this build";
        }

        const bool exact{ core::media::getCodecDesc(testCase.codec).isLossless }; // compare decoded samples bit-for-bit instead of just checking duration

        ffmpeg::AudioEncoder encoder{ createParameters(testCase.container, testCase.codec) };
        const PcmParameters inputParameters{ encoder.getInputParameters() };
        if (exact)
        {
            ASSERT_EQ(inputParameters.sampleType, PcmSampleType::Signed16) << "exact-sample comparison below assumes 16-bit PCM";
        }

        const GeneratedSamples samples{ generateSineWave(inputParameters, defaultDuration) };
        const std::vector<std::byte> output{ encode(encoder, samples) };
        ASSERT_FALSE(output.empty());

        if (!isDecodingSupported(testCase.container, testCase.codec))
        {
            GTEST_SKIP() << "decoding not supported by this build: only the encoding path above was verified";
        }

        const ScopedTmpDirectory directory;
        const std::filesystem::path outputPath{ directory / ("output." + testCase.name) };
        writeFile(outputPath, output);

        const std::vector<std::int16_t> decoded{ decodeInterleaved(outputPath, inputParameters.channelCount, inputParameters.sampleRate) };
        ASSERT_FALSE(decoded.empty());

        if (exact)
        {
            const std::vector<std::int16_t> inputSamples{ toInterleavedInt16(samples, inputParameters.channelCount, inputParameters.planar) };

            ASSERT_EQ(decoded.size(), inputSamples.size());
            EXPECT_EQ(decoded, inputSamples);
        }
        else
        {
            EXPECT_NEAR(static_cast<double>(decoded.size() / inputParameters.channelCount) / inputParameters.sampleRate, std::chrono::duration<double>{ defaultDuration }.count(), 0.2);
        }
    }

    INSTANTIATE_TEST_SUITE_P(
        audio,
        AudioEncoder,
        ::testing::ValuesIn(buildRoundTripTestCases()),
        [](const ::testing::TestParamInfo<RoundTripTestCase>& info) { return info.param.name; });

    TEST(AudioEncoder, opusSnapsToASupportedSampleRate)
    {
        // Opus only supports 8/12/16/24/48 kHz: a request for 44100 has to be snapped to the next supported rate
        const ffmpeg::AudioEncoder encoder{ createParameters(core::media::Container::Ogg, core::media::Codec::Opus, 2, 44'100) };
        EXPECT_EQ(encoder.getInputParameters().sampleRate, 48'000u);
    }

    TEST(AudioEncoder, mp3ChannelCountIsLimitedToWhatTheEncoderSupports)
    {
        const ffmpeg::AudioEncoder encoder{ createParameters(core::media::Container::MPEG, core::media::Codec::MP3, 6) };
        EXPECT_EQ(encoder.getInputParameters().channelCount, 2u);
    }

    TEST(AudioEncoder, findMuxerMatchesCapabilityForEveryContainerCodecPair)
    {
        core::media::visitContainerCodecPairs([](const core::media::ContainerCodec& pair) {
            const std::string caseName{ std::string{ core::media::containerToString(pair.container).str() } + "_" + std::string{ core::media::getCodecDesc(pair.codec).name.str() } };
            if (ffmpeg::utils::isCodecMuxingSupported(pair.container, pair.codec))
                EXPECT_NO_THROW(ffmpeg::utils::findMuxer(pair.container, pair.codec)) << caseName;
            else
                EXPECT_ANY_THROW(ffmpeg::utils::findMuxer(pair.container, pair.codec)) << caseName;
        });
    }

} // namespace lms::audio::tests
