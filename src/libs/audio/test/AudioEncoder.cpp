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
#include <fstream>
#include <numbers>
#include <ostream>
#include <random>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "core/media/Codec.hpp"
#include "core/media/ContainerCodec.hpp"

#include "audio/IAudioDecoder.hpp"
#include "audio/PcmTypes.hpp"

#include "ffmpeg/AudioEncoder.hpp"
#include "ffmpeg/EncoderUtils.hpp"
#include "ffmpeg/Utils.hpp"

namespace lms::audio::tests
{
    namespace
    {
        constexpr std::chrono::milliseconds defaultDuration{ 3'000 };

        class ScopedDirectory
        {
        public:
            ScopedDirectory()
                : _path{ std::filesystem::temp_directory_path() / ("lms-audioencoder-test-" + std::to_string(std::random_device{}())) }
            {
                std::filesystem::create_directories(_path);
            }

            ~ScopedDirectory()
            {
                std::error_code ec;
                std::filesystem::remove_all(_path, ec);
            }

            ScopedDirectory(const ScopedDirectory&) = delete;
            ScopedDirectory& operator=(const ScopedDirectory&) = delete;

            std::filesystem::path operator/(std::string_view fileName) const { return _path / fileName; }

        private:
            std::filesystem::path _path;
        };

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

        void writeFile(const std::filesystem::path& path, std::span<const std::byte> data)
        {
            std::ofstream os{ path, std::ios::binary };
            os.write(static_cast<const char*>(static_cast<const void*>(data.data())), static_cast<std::streamsize>(data.size()));
        }

        std::vector<std::int16_t> decodeInterleaved(const std::filesystem::path& path, unsigned channelCount, unsigned sampleRate)
        {
            const PcmParameters pcmParameters{
                .channelCount = channelCount,
                .sampleRate = sampleRate,
                .sampleType = PcmSampleType::Signed16,
                .byteOrder = std::endian::native,
                .planar = false,
            };

            const std::unique_ptr<IAudioDecoder> decoder{ createAudioDecoder(path, std::chrono::microseconds{ 0 }, pcmParameters) };

            constexpr std::size_t decodeSampleCount{ 8'192 };
            std::vector<std::byte> buffer(decodeSampleCount * channelCount * sizeof(std::int16_t));
            std::array<IAudioDecoder::WritableBuffer, 1> buffers{ std::span{ buffer } };

            std::vector<std::int16_t> res;
            while (!decoder->finished())
            {
                const std::size_t sampleCount{ decoder->readSamples(buffers) };
                if (sampleCount == 0)
                    break;

                const auto* samples{ static_cast<const std::int16_t*>(static_cast<const void*>(buffer.data())) };
                res.insert(std::end(res), samples, samples + sampleCount * channelCount);
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

        // Real-world container/codec pairings only (see core::media::visitContainerCodecPairs): whichever
        // of these this build's ffmpeg can't actually mux/encode are skipped at test time via
        // isCodecMuxingSupported, not filtered out here, so coverage grows automatically as ffmpeg gains
        // capabilities.
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

    class AudioEncoderRoundTrip : public ::testing::TestWithParam<RoundTripTestCase>
    {
    };

    TEST_P(AudioEncoderRoundTrip, roundTrips)
    {
        const RoundTripTestCase& testCase{ GetParam() };

        // AudioEncoder's output is intentionally not seekable (streaming, like piping out the ffmpeg
        // CLI's output), but AIFF/MP4/MOV-family muxers need to seek back to finalize chunk/atom sizes
        // once all data is written. Nothing to do with codec/container compatibility: even a plain
        // `ffmpeg -f aiff -`/`-f mp4 -` piped to stdout hits the same limitation.
        if (testCase.container == core::media::Container::AIFF || testCase.container == core::media::Container::MP4)
        {
            GTEST_SKIP() << "container requires seekable output, which this encoder does not provide";
        }

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

        const ScopedDirectory directory;
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
        Formats,
        AudioEncoderRoundTrip,
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

    // findEncoder must resolve every codec this build's ffmpeg can actually encode, and only those
    TEST(AudioEncoder, findEncoderMatchesCapabilityForEveryCodec)
    {
        core::media::visitCodecs([](const core::media::CodecDesc& desc) {
            if (ffmpeg::utils::isEncodingSupported(desc.type))
                EXPECT_NO_THROW(ffmpeg::utils::findEncoder(desc.type)) << desc.name.str();
            else
                EXPECT_ANY_THROW(ffmpeg::utils::findEncoder(desc.type)) << desc.name.str();
        });
    }

    // getMuxerName must resolve every container this build's ffmpeg can actually mux, and only those
    TEST(AudioEncoder, getMuxerNameMatchesCapabilityForEveryContainer)
    {
        core::media::visitContainers([](core::media::Container container) {
            if (ffmpeg::utils::isMuxingSupported(container))
                EXPECT_NO_THROW(ffmpeg::utils::getMuxerName(container)) << core::media::containerToString(container).str();
            else
                EXPECT_ANY_THROW(ffmpeg::utils::getMuxerName(container)) << core::media::containerToString(container).str();
        });
    }

} // namespace lms::audio::tests
