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
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <random>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "audio/IAudioDecoder.hpp"
#include "audio/PcmTypes.hpp"

#include "ffmpeg/AudioEncoder.hpp"

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

        bool startsWith(std::span<const std::byte> data, std::string_view magic)
        {
            return data.size() >= magic.size() && std::memcmp(data.data(), magic.data(), magic.size()) == 0;
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
    } // namespace

    TEST(AudioEncoder, roundTripsMp3)
    {
        ffmpeg::AudioEncoder encoder{ createParameters(core::media::Container::MPEG, core::media::Codec::MP3) };
        const PcmParameters inputParameters{ encoder.getInputParameters() };
        const GeneratedSamples samples{ generateSineWave(inputParameters, defaultDuration) };
        const std::vector<std::byte> output{ encode(encoder, samples) };
        ASSERT_FALSE(output.empty());

        const ScopedDirectory directory;
        const std::filesystem::path outputPath{ directory / "output.mp3" };
        writeFile(outputPath, output);

        const std::vector<std::int16_t> decoded{ decodeInterleaved(outputPath, inputParameters.channelCount, inputParameters.sampleRate) };
        ASSERT_FALSE(decoded.empty());

        // MP3 has an encoder delay that cannot be signalled without a seekable output
        EXPECT_NEAR(static_cast<double>(decoded.size() / inputParameters.channelCount) / inputParameters.sampleRate, std::chrono::duration<double>{ defaultDuration }.count(), 0.2);
    }

    TEST(AudioEncoder, roundTripsOggVorbis)
    {
        ffmpeg::AudioEncoder encoder{ createParameters(core::media::Container::Ogg, core::media::Codec::Vorbis) };
        const PcmParameters inputParameters{ encoder.getInputParameters() };
        const GeneratedSamples samples{ generateSineWave(inputParameters, defaultDuration) };
        const std::vector<std::byte> output{ encode(encoder, samples) };
        ASSERT_FALSE(output.empty());
        EXPECT_TRUE(startsWith(output, "OggS"));

        const ScopedDirectory directory;
        const std::filesystem::path outputPath{ directory / "output.ogg" };
        writeFile(outputPath, output);

        const std::vector<std::int16_t> decoded{ decodeInterleaved(outputPath, inputParameters.channelCount, inputParameters.sampleRate) };
        EXPECT_NEAR(static_cast<double>(decoded.size() / inputParameters.channelCount) / inputParameters.sampleRate, std::chrono::duration<double>{ defaultDuration }.count(), 0.1);
    }

    TEST(AudioEncoder, flacIsLossless)
    {
        ffmpeg::AudioEncoder encoder{ createParameters(core::media::Container::FLAC, core::media::Codec::FLAC) };
        const PcmParameters inputParameters{ encoder.getInputParameters() };
        ASSERT_FALSE(inputParameters.planar) << "exact-sample comparison below assumes interleaved input";
        ASSERT_EQ(inputParameters.sampleType, PcmSampleType::Signed16) << "exact-sample comparison below assumes 16-bit PCM";

        const GeneratedSamples samples{ generateSineWave(inputParameters, defaultDuration) };
        const std::vector<std::byte> output{ encode(encoder, samples) };
        ASSERT_FALSE(output.empty());
        EXPECT_TRUE(startsWith(output, "fLaC"));

        const ScopedDirectory directory;
        const std::filesystem::path outputPath{ directory / "output.flac" };
        writeFile(outputPath, output);

        const std::vector<std::int16_t> decoded{ decodeInterleaved(outputPath, inputParameters.channelCount, inputParameters.sampleRate) };

        const std::vector<std::byte>& inputBytes{ samples.channelBuffers.at(0) };
        std::vector<std::int16_t> inputSamples(inputBytes.size() / sizeof(std::int16_t));
        std::memcpy(inputSamples.data(), inputBytes.data(), inputBytes.size());

        ASSERT_EQ(decoded.size(), inputSamples.size());
        EXPECT_EQ(decoded, inputSamples);
    }

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
} // namespace lms::audio::tests
