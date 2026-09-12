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

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <boost/asio/io_context.hpp>

#include "core/media/AudioFormat.hpp"
#include "core/media/MimeType.hpp"

#include "audio/Exception.hpp"
#include "audio/IAudioDecoder.hpp"
#include "audio/IAudioFileInfo.hpp"
#include "audio/IAudioFileInfoParser.hpp"
#include "audio/ITagReader.hpp"
#include "audio/ITranscoder.hpp"
#include "audio/PcmTypes.hpp"

#include "TestData.hpp"
#include "TestUtils.hpp"

namespace lms::audio::tests
{
    namespace
    {
        constexpr std::chrono::milliseconds offsetDuration{ 1'000 };

        // Not consumed by the transcode path itself: only channelCount/sampleRate feed the encoder negotiation
        AudioProperties toAudioProperties(const TestAudioFile& file)
        {
            return AudioProperties{
                .container = file.container,
                .codec = file.codec,
                .duration = file.duration,
                .bitrate = 128'000,
                .channelCount = file.channelCount,
                .sampleRate = file.sampleRate,
                .bitsPerSample = std::nullopt,
            };
        }

        TranscodeParameters createParameters(const std::filesystem::path& inputPath, const AudioProperties& audioProperties, core::media::Container outputContainer, core::media::Codec outputCodec)
        {
            TranscodeParameters parameters;
            parameters.inputParameters.filePath = inputPath;
            parameters.inputParameters.audioProperties = audioProperties;
            parameters.outputParameters.format = TranscodeOutputFormat{ .container = outputContainer, .codec = outputCodec };
            parameters.outputParameters.bitrate = 128'000;
            parameters.outputParameters.stripMetadata = false;

            return parameters;
        }

        TranscodeParameters createParameters(const TestAudioFile& file, core::media::Container outputContainer, core::media::Codec outputCodec)
        {
            return createParameters(file.getPath(), toAudioProperties(file), outputContainer, outputCodec);
        }

        const TestAudioFile& getFirstSupportedTestAudioFile()
        {
            for (const TestAudioFile& file : getTestAudioFiles())
            {
                if (isDecodingSupported(file.container, file.codec))
                    return file;
            }

            throw Exception{ "No test audio file is decodable by this build" };
        }

        struct SupportedOutputFormat
        {
            core::media::Container container;
            core::media::Codec codec;
            std::string_view extension; // leading dot, ex ".flac"
        };

        const SupportedOutputFormat& getFirstSupportedLosslessOutputFormat()
        {
            static const std::optional<SupportedOutputFormat> format = [] {
                std::optional<SupportedOutputFormat> result;
                core::media::visitAudioFormats([&](const core::media::AudioFormat& candidate, core::media::ExtensionSpan extensions) {
                    if (!core::media::getCodecDesc(candidate.codec).isLossless || !isEncodingSupported(candidate.container, candidate.codec))
                        return core::Continue;

                    result = SupportedOutputFormat{ candidate.container, candidate.codec, extensions.front() };
                    return core::Break;
                });
                return result;
            }();

            if (!format)
                throw Exception{ "No output format is supported by this build" };

            return *format;
        }

        std::vector<std::byte> transcode(const TranscodeParameters& parameters)
        {
            boost::asio::io_context ioContext;
            const std::unique_ptr<ITranscoder> transcoder{ createTranscoder(ioContext, parameters) };

            std::vector<std::byte> res;
            std::array<std::byte, 65'536> buffer{};

            while (!transcoder->finished())
            {
                const std::size_t byteCount{ transcoder->readSome(buffer.data(), buffer.size()) };
                if (byteCount == 0)
                    break;

                res.insert(std::end(res), std::cbegin(buffer), std::cbegin(buffer) + byteCount);
            }

            return res;
        }

        std::string readTagValue(const std::filesystem::path& path, TagType tag)
        {
            const std::unique_ptr<IAudioFileInfo> audioFileInfo{ createAudioFileInfoParser(AudioFileInfoParserBackend::TagLib)->parse(path) };
            if (!audioFileInfo)
                return {};

            const ITagReader* tagReader{ audioFileInfo->getTagReader() };
            if (!tagReader)
                return {};

            std::string res;
            tagReader->visitTagValues(tag, [&](std::string_view value) { res = value; });

            return res;
        }

        struct TargetFormat
        {
            std::string_view name; // valid gtest identifier, also used as the output file extension
            core::media::Container container;
            core::media::Codec codec;
        };

        std::ostream& operator<<(std::ostream& os, const TargetFormat& format)
        {
            return os << format.name;
        }

        // One representative target per tag system taglib writes: ID3v2 (MP3), Vorbis comments (Ogg/FLAC)
        constexpr std::array targetFormats{
            TargetFormat{ "mp3", core::media::Container::MPEG, core::media::Codec::MP3 },
            TargetFormat{ "oggVorbis", core::media::Container::Ogg, core::media::Codec::Vorbis },
            TargetFormat{ "flac", core::media::Container::FLAC, core::media::Codec::FLAC },
        };

        using MetadataParam = std::tuple<TestAudioFile, TargetFormat>;

        std::string metadataParamName(const ::testing::TestParamInfo<MetadataParam>& info)
        {
            return std::string{ std::get<0>(info.param).name } + "_" + std::string{ std::get<1>(info.param).name };
        }
    } // namespace

    // Every test audio file carries the same known title/artist tags
    class TranscoderMetadata : public ::testing::TestWithParam<MetadataParam>
    {
    };

    TEST_P(TranscoderMetadata, tagsArePropagated)
    {
        const auto& [file, targetFormat]{ GetParam() };

        if (!isDecodingSupported(file.container, file.codec) || !isEncodingSupported(targetFormat.container, targetFormat.codec))
        {
            GTEST_SKIP() << "container/codec combination not supported by this build";
        }

        const ScopedTmpDirectory directory;
        const std::vector<std::byte> output{ transcode(createParameters(file, targetFormat.container, targetFormat.codec)) };
        ASSERT_FALSE(output.empty());

        const std::filesystem::path outputPath{ directory / ("output." + std::string{ targetFormat.name }) };
        writeFile(outputPath, output);

        EXPECT_EQ(readTagValue(outputPath, TagType::TrackTitle), testTagTitle.str());
        EXPECT_EQ(readTagValue(outputPath, TagType::Artist), testTagArtist.str());
    }

    INSTANTIATE_TEST_SUITE_P(
        audio,
        TranscoderMetadata,
        ::testing::Combine(::testing::ValuesIn(getTestAudioFiles()), ::testing::ValuesIn(targetFormats)),
        &metadataParamName);

    TEST(Transcoder, stripMetadataRemovesTags)
    {
        const ScopedTmpDirectory directory;
        const TestAudioFile& file{ getFirstSupportedTestAudioFile() };
        const SupportedOutputFormat& target{ getFirstSupportedLosslessOutputFormat() };

        TranscodeParameters parameters{ createParameters(file, target.container, target.codec) };
        parameters.outputParameters.stripMetadata = true;

        const std::filesystem::path outputPath{ directory / ("stripped" + std::string{ target.extension }) };
        writeFile(outputPath, transcode(parameters));

        EXPECT_EQ(readTagValue(outputPath, TagType::TrackTitle), "");
    }

    // Verifies AudioDecoder's seek+trim logic (computeStartTrimSampleCount) end to end
    class TranscoderOffset : public ::testing::TestWithParam<TestAudioFile>
    {
    };

    TEST_P(TranscoderOffset, offsetIsSampleAccurate)
    {
        const TestAudioFile& file{ GetParam() };

        if (!isDecodingSupported(file.container, file.codec))
        {
            GTEST_SKIP() << "container/codec combination not supported by this build";
        }

        const std::vector<std::int16_t> groundTruth{ decodeInterleaved(file.getPath(), file.channelCount, file.sampleRate) };
        ASSERT_FALSE(groundTruth.empty());

        const std::size_t skippedSampleCount{ helpers::durationToSampleCount(offsetDuration, file.sampleRate) * file.channelCount };
        ASSERT_GT(groundTruth.size(), skippedSampleCount);

        const SupportedOutputFormat& target{ getFirstSupportedLosslessOutputFormat() };
        TranscodeParameters parameters{ createParameters(file, target.container, target.codec) };
        parameters.inputParameters.offset = offsetDuration;

        const ScopedTmpDirectory directory;
        const std::filesystem::path outputPath{ directory / ("output" + std::string{ target.extension }) };
        writeFile(outputPath, transcode(parameters));

        const std::vector<std::int16_t> offsetSamples{ decodeInterleaved(outputPath, file.channelCount, file.sampleRate) };
        ASSERT_FALSE(offsetSamples.empty());

        if (core::media::getCodecDesc(file.codec).isLossless)
        {
            const std::size_t compareCount{ std::min<std::size_t>(1'000, offsetSamples.size()) };
            for (std::size_t i{}; i < compareCount; ++i)
                ASSERT_EQ(offsetSamples[i], groundTruth[skippedSampleCount + i]) << "sample index " << i;
        }
        else
        {
            // Lossy codecs don't guarantee byte-exact re-decode around an arbitrary seek point
            const auto expectedDuration{ file.duration - offsetDuration };
            EXPECT_NEAR(static_cast<double>(offsetSamples.size() / file.channelCount) / file.sampleRate, std::chrono::duration<double>{ expectedDuration }.count(), 0.2);
        }
    }

    INSTANTIATE_TEST_SUITE_P(
        audio,
        TranscoderOffset,
        ::testing::ValuesIn(getTestAudioFiles()),
        [](const ::testing::TestParamInfo<TestAudioFile>& info) { return std::string{ info.param.name }; });

    TEST(Transcoder, asyncReadMatchesSyncRead)
    {
        const TestAudioFile& file{ getFirstSupportedTestAudioFile() };
        const SupportedOutputFormat& target{ getFirstSupportedLosslessOutputFormat() };
        const TranscodeParameters parameters{ createParameters(file, target.container, target.codec) };
        const std::vector<std::byte> expectedOutput{ transcode(parameters) };

        boost::asio::io_context ioContext;
        const std::unique_ptr<ITranscoder> transcoder{ createTranscoder(ioContext, parameters) };

        std::vector<std::byte> output;
        std::array<std::byte, 65'536> buffer{};

        std::function<void()> readNext{ [&] {
            transcoder->asyncRead(buffer.data(), buffer.size(), [&](std::size_t byteCount) {
                output.insert(std::end(output), std::cbegin(buffer), std::cbegin(buffer) + byteCount);
                if (byteCount > 0 && !transcoder->finished())
                    readNext();
            });
        } };

        readNext();
        ioContext.run();

        EXPECT_EQ(output, expectedOutput);
    }

    TEST(Transcoder, abortedAsyncReadDoesNotCallBack)
    {
        const TestAudioFile& file{ getFirstSupportedTestAudioFile() };
        const SupportedOutputFormat& target{ getFirstSupportedLosslessOutputFormat() };

        boost::asio::io_context ioContext;
        std::unique_ptr<ITranscoder> transcoder{ createTranscoder(ioContext, createParameters(file, target.container, target.codec)) };

        std::array<std::byte, 65'536> buffer{};
        bool callbackCalled{};
        transcoder->asyncRead(buffer.data(), buffer.size(), [&](std::size_t) { callbackCalled = true; });

        transcoder.reset();
        ioContext.run();

        EXPECT_FALSE(callbackCalled);
    }

    TEST(Transcoder, outputMimeType)
    {
        const TestAudioFile& file{ getFirstSupportedTestAudioFile() };
        const SupportedOutputFormat& target{ getFirstSupportedLosslessOutputFormat() };
        boost::asio::io_context ioContext;

        const std::unique_ptr<ITranscoder> transcoder{ createTranscoder(ioContext, createParameters(file, target.container, target.codec)) };
        EXPECT_EQ(transcoder->getOutputMimeType(), core::media::getMimeType(target.container, target.codec).str());
    }

    TEST(Transcoder, missingInputFileThrows)
    {
        const ScopedTmpDirectory directory;
        const TestAudioFile& file{ getFirstSupportedTestAudioFile() };
        const SupportedOutputFormat& target{ getFirstSupportedLosslessOutputFormat() };

        boost::asio::io_context ioContext;
        EXPECT_THROW(createTranscoder(ioContext, createParameters(directory / "missing.wav", toAudioProperties(file), target.container, target.codec)), Exception);
    }

    TEST(Transcoder, missingOutputFormatThrows)
    {
        const TestAudioFile& file{ getFirstSupportedTestAudioFile() };
        const SupportedOutputFormat& target{ getFirstSupportedLosslessOutputFormat() };

        TranscodeParameters parameters{ createParameters(file, target.container, target.codec) };
        parameters.outputParameters.format.reset();

        boost::asio::io_context ioContext;
        EXPECT_THROW(createTranscoder(ioContext, parameters), Exception);
    }
} // namespace lms::audio::tests
