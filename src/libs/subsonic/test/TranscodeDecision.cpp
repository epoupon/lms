/*
 * Copyright (C) 2025 Emeric Poupon
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
#include <variant>

#include "av/IAudioFile.hpp"
#include "core/Utils.hpp"

#include "endpoints/transcoding/TranscodeDecision.hpp"
#include "responses/ClientInfo.hpp"

namespace lms::api::subsonic
{
    namespace details
    {
        std::string_view transcodeReasonToString(TranscodeReason reason)
        {
            switch (reason)
            {
            case TranscodeReason::AudioCodecNotSupported:
                return "audio codec not supported";
            case TranscodeReason::AudioBitrateNotSupported:
                return "audio bitrate not supported";
            case TranscodeReason::AudioChannelsNotSupported:
                return "audio channels not supported";
            case TranscodeReason::AudioSampleRateNotSupported:
                return "audio samplerate not supported";
            case TranscodeReason::AudioBitdepthNotSupported:
                return "audio bitdepth not supported";
            case TranscodeReason::ContainerNotSupported:
                return "container not supported";
            case TranscodeReason::ProtocolNotSupported:
                return "protocol not supported";
            }

            return "unknown";
        }

        std::ostream& operator<<(std::ostream& os, const details::TranscodeDecisionResult& result)
        {
            std::visit(core::utils::overloads{
                           [&](const details::DirectPlayResult&) { os << "direct play"; },
                           [&](const details::FailureResult& res) { os << "failure: " << res.reason; },
                           [&](const details::TranscodeResult& res) {
                               os << "transcode: reasons = {";

                               bool firstReason{ true };
                               for (TranscodeReason reason : res.reasons)
                               {
                                   if (!firstReason)
                                       os << ", ";
                                   os << transcodeReasonToString(reason);
                                   firstReason = false;
                               }
                               os << "}, stream = {";
                               os << "protocol = " << res.targetStreamInfo.protocol << ", container = " << res.targetStreamInfo.container << ", codec = " << res.targetStreamInfo.codec;
                               if (res.targetStreamInfo.audioChannels)
                                   os << ", audioChannels = " << *res.targetStreamInfo.audioChannels;
                               if (res.targetStreamInfo.audioBitrate)
                                   os << ", audioBitrate = " << *res.targetStreamInfo.audioBitrate;
                               os << ", audioProfile = " << res.targetStreamInfo.audioProfile;
                               if (res.targetStreamInfo.audioSamplerate)
                                   os << ", audioSamplerate = " << *res.targetStreamInfo.audioSamplerate;
                               if (res.targetStreamInfo.audioBitdepth)
                                   os << ", audioBitdepth = " << *res.targetStreamInfo.audioBitdepth;
                               os << "}";
                           } },
                       result);

            return os;
        } // namespace
    }; // namespace details

    namespace
    {

        struct TestCase
        {
            ClientInfo clientInfo;
            av::ContainerInfo containerInfo;
            av::StreamInfo audioStreamInfo;

            details::TranscodeDecisionResult expected;
        };

        void processTests(std::span<const TestCase> testCases)
        {
            for (std::size_t testCaseIndex{ 0 }; testCaseIndex < std::size(testCases); ++testCaseIndex)
            {
                const auto& testCase{ testCases[testCaseIndex] };
                const details::TranscodeDecisionResult decision{ details::computeTranscodeDecision(testCase.clientInfo, testCase.containerInfo, testCase.audioStreamInfo) };

                EXPECT_EQ(testCase.expected, decision) << "testCaseIndex: " << testCaseIndex;
            }
        }
    } // namespace

    TEST(TranscodeDecision, directPlay)
    {
        const TestCase testCases[]{
            // Direct play
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 512'000,
                    .maxTranscodingAudioBitrate = 256'000,
                    .directPlayProfiles = { {
                        { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocol = "http", .maxAudioChannels = 2 },
                    } },
                    .transcodingProfiles = {
                        { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = 2 },
                    },
                    .codecProfiles = { { .type = "AudioCodec", .name = "mp3", .limitations = {
                                                                                  { .name = Limitation::Type::AudioBitrate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "256000" }, .required = true },
                                                                              } } },
                },
                .containerInfo = { .bitrate = 128'000, .name = "mp3" },
                .audioStreamInfo = {
                    .index = 0,
                    .bitrate = 128'000,
                    .bitsPerSample = 16,
                    .channelCount = 2,
                    .sampleRate = 44'100,
                    .codec = av::DecodingCodec::MP3,
                    .codecName = "mp3",
                },

                .expected = { details::DirectPlayResult{} },
            },

            // Needs transcode due to codec limitation
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 512'000,
                    .maxTranscodingAudioBitrate = 96'000,
                    .directPlayProfiles = { {
                        { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocol = "http", .maxAudioChannels = 2 },
                    } },
                    .transcodingProfiles = {
                        { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = 2 },
                    },
                    .codecProfiles = { { .type = "AudioCodec", .name = "mp3", .limitations = {
                                                                                  { .name = Limitation::Type::AudioBitrate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "96000" }, .required = true },
                                                                              } } },
                },
                .containerInfo = { .bitrate = 128'000, .name = "mp3" },
                .audioStreamInfo = {
                    .index = 0,
                    .bitrate = 128'000,
                    .bitsPerSample = 16,
                    .channelCount = 2,
                    .sampleRate = 44'100,
                    .codec = av::DecodingCodec::MP3,
                    .codecName = "mp3",
                },

                .expected = { details::TranscodeResult{ .reasons = { details::TranscodeReason::AudioBitrateNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "mp3", .codec = "mp3", .audioChannels = std::nullopt, .audioBitrate = 96000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },

            // Needs transcode due to global limitation on the direct play bitrate
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 96'000,
                    .maxTranscodingAudioBitrate = 96'000,
                    .directPlayProfiles = { {
                        { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocol = "http", .maxAudioChannels = 2 },
                    } },
                    .transcodingProfiles = {
                        { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = 2 },
                    },
                    .codecProfiles = { { .type = "AudioCodec", .name = "mp3", .limitations = {
                                                                                  { .name = Limitation::Type::AudioBitrate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "256000" }, .required = true },
                                                                              } } },
                },
                .containerInfo = { .bitrate = 128'000, .name = "mp3" },
                .audioStreamInfo = {
                    .index = 0,
                    .bitrate = 128'000,
                    .bitsPerSample = 16,
                    .channelCount = 2,
                    .sampleRate = 44'100,
                    .codec = av::DecodingCodec::MP3,
                    .codecName = "mp3",
                },

                .expected = { details::TranscodeResult{ .reasons = { details::TranscodeReason::AudioBitrateNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "mp3", .codec = "mp3", .audioChannels = std::nullopt, .audioBitrate = 96000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },

            // Needs transcode due to codec limitation, but global limitation is even more restrictive
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 96'000,
                    .maxTranscodingAudioBitrate = 96'000,
                    .directPlayProfiles = { {
                        { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocol = "http", .maxAudioChannels = 2 },
                    } },
                    .transcodingProfiles = {
                        { "mp3", "mp3", "http", 2 },
                    },
                    .codecProfiles = { { .type = "AudioCodec", .name = "mp3", .limitations = {
                                                                                  { .name = Limitation::Type::AudioBitrate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "128000" }, .required = true },
                                                                              } } },
                },
                .containerInfo = { .bitrate = 128'000, .name = "mp3" },
                .audioStreamInfo = {
                    .index = 0,
                    .bitrate = 192'000,
                    .bitsPerSample = 16,
                    .channelCount = 2,
                    .sampleRate = 44'100,
                    .codec = av::DecodingCodec::MP3,
                    .codecName = "mp3",
                },

                .expected = { details::TranscodeResult{ .reasons = { details::TranscodeReason::AudioBitrateNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "mp3", .codec = "mp3", .audioChannels = std::nullopt, .audioBitrate = 96'000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },

            // Needs transcode due to max audio sample rate not handle by codec limitation
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 320'000,
                    .maxTranscodingAudioBitrate = 320'000,
                    .directPlayProfiles = { {
                        { { "mp3" }, { "mp3" }, "http", 2 },
                    } },
                    .transcodingProfiles = {
                        { "mp3", "mp3", "http", 2 },
                    },
                    .codecProfiles = { { .type = "AudioCodec", .name = "mp3", .limitations = {
                                                                                  { .name = Limitation::Type::AudioSamplerate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "48000" }, .required = true },
                                                                              } } },
                },
                .containerInfo = { .bitrate = 128'000, .name = "mp3" },
                .audioStreamInfo = {
                    .index = 0,
                    .bitrate = 192'000,
                    .bitsPerSample = 16,
                    .channelCount = 2,
                    .sampleRate = 96'000,
                    .codec = av::DecodingCodec::MP3,
                    .codecName = "mp3",
                },

                .expected = { details::TranscodeResult{ .reasons = { details::TranscodeReason::AudioSampleRateNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "mp3", .codec = "mp3", .audioChannels = std::nullopt, .audioBitrate = 192'000, .audioProfile = "", .audioSamplerate = 48'000, .audioBitdepth = std::nullopt } } },
            },

            // Needs transcode due to max nb channels not handle by profile
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 320'000,
                    .maxTranscodingAudioBitrate = 320'000,
                    .directPlayProfiles = { { { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocol = "http", .maxAudioChannels = 2 } } },
                    .transcodingProfiles = { { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = 2 } },
                    .codecProfiles = { { .type = "AudioCodec", .name = "mp3", .limitations = {} } },
                },
                .containerInfo = { .bitrate = 128'000, .name = "mp3" },
                .audioStreamInfo = {
                    .index = 0,
                    .bitrate = 192'000,
                    .bitsPerSample = 16,
                    .channelCount = 5,
                    .sampleRate = 48'000,
                    .codec = av::DecodingCodec::MP3,
                    .codecName = "mp3",
                },

                .expected = { details::TranscodeResult{ .reasons = { details::TranscodeReason::AudioChannelsNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "mp3", .codec = "mp3", .audioChannels = 2, .audioBitrate = 192'000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },

            // Needs transcode due to max nb channels not handle by codec. TODO take channel reduction into account for bitrate
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 320'000,
                    .maxTranscodingAudioBitrate = 320'000,
                    .directPlayProfiles = { {
                        { { "mp3" }, { "mp3" }, "http", std::nullopt },
                    } },
                    .transcodingProfiles = {
                        { "mp3", "mp3", "http", std::nullopt },
                    },
                    .codecProfiles = { { .type = "AudioCodec", .name = "mp3", .limitations = { { .name = Limitation::Type::AudioChannels, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "2" }, .required = true } } } },
                },
                .containerInfo = { .bitrate = 128'000, .name = "mp3" },
                .audioStreamInfo = {
                    .index = 0,
                    .bitrate = 192'000,
                    .bitsPerSample = 16,
                    .channelCount = 5,
                    .sampleRate = 48'000,
                    .codec = av::DecodingCodec::MP3,
                    .codecName = "mp3",
                },

                .expected = { details::TranscodeResult{ .reasons = { details::TranscodeReason::AudioChannelsNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "mp3", .codec = "mp3", .audioChannels = 2, .audioBitrate = 192'000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },

            // needs transcode because codec not handled
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 320'000,
                    .maxTranscodingAudioBitrate = 320'000,
                    .directPlayProfiles = { {
                        { { "mp3" }, { "mp3" }, "http", std::nullopt },
                    } },
                    .transcodingProfiles = {
                        { "mp3", "mp3", "http", std::nullopt },
                    },
                    .codecProfiles = { { .type = "AudioCodec", .name = "mp3", .limitations = { { .name = Limitation::Type::AudioChannels, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "2" }, .required = true } } } },
                },
                .containerInfo = { .bitrate = 128'000, .name = "mp3" },
                .audioStreamInfo = {
                    .index = 0,
                    .bitrate = 128'000,
                    .bitsPerSample = 16,
                    .channelCount = 2,
                    .sampleRate = 48'000,
                    .codec = av::DecodingCodec::OPUS,
                    .codecName = "opus",
                },

                .expected = { details::TranscodeResult{ .reasons = { details::TranscodeReason::AudioCodecNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "mp3", .codec = "mp3", .audioChannels = std::nullopt, .audioBitrate = 128'000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },

            // needs transcode because codec not handled (lossless source => using max bitrate)
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 1'000'000,
                    .maxTranscodingAudioBitrate = 320'000,
                    .directPlayProfiles = { {
                        { { "mp3" }, { "mp3" }, "http", std::nullopt },
                    } },
                    .transcodingProfiles = {
                        { "mp3", "mp3", "http", std::nullopt },
                    },
                    .codecProfiles = { { .type = "AudioCodec", .name = "mp3", .limitations = {} } },
                },
                .containerInfo = { .bitrate = 128'000, .name = "mp3" },
                .audioStreamInfo = {
                    .index = 0,
                    .bitrate = 750'000,
                    .bitsPerSample = 16,
                    .channelCount = 2,
                    .sampleRate = 48'000,
                    .codec = av::DecodingCodec::FLAC,
                    .codecName = "flac",
                },

                .expected = { details::TranscodeResult{ .reasons = { details::TranscodeReason::AudioCodecNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "mp3", .codec = "mp3", .audioChannels = std::nullopt, .audioBitrate = 320000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },

            // needs transcode because codec not handled (lossless source => using a default good bitrate)
            {
                .clientInfo = { .name = "TestClient", .platform = "TestPlatform", .maxAudioBitrate = std::nullopt, .maxTranscodingAudioBitrate = std::nullopt, .directPlayProfiles = { {
                                                                                                                                                                   { { "mp3" }, { "mp3" }, "http", std::nullopt },
                                                                                                                                                               } },
                                .transcodingProfiles = {
                                    { "mp3", "mp3", "http", std::nullopt },
                                },
                                .codecProfiles = {} },
                .containerInfo = { .bitrate = 128000, .name = "mp3" },
                .audioStreamInfo = {
                    .index = 0,
                    .bitrate = 750000,
                    .bitsPerSample = 16,
                    .channelCount = 2,
                    .sampleRate = 48000,
                    .codec = av::DecodingCodec::FLAC,
                    .codecName = "flac",
                },

                .expected = { details::TranscodeResult{ .reasons = { details::TranscodeReason::AudioCodecNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "mp3", .codec = "mp3", .audioChannels = std::nullopt, .audioBitrate = 256000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },

            // check protocol * and codec * are properly handled
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 1'000'000,
                    .maxTranscodingAudioBitrate = 320'000,
                    .directPlayProfiles = { {
                        { { "mp4", "flac", "mp3" }, { "*" }, "*", std::nullopt },
                    } },
                    .transcodingProfiles = {},
                    .codecProfiles = {},
                },
                .containerInfo = { .bitrate = 128'000, .name = "mp3" },
                .audioStreamInfo = {
                    .index = 0,
                    .bitrate = 128'000,
                    .bitsPerSample = 16,
                    .channelCount = 2,
                    .sampleRate = 48'000,
                    .codec = av::DecodingCodec::MP3,
                    .codecName = "mp3",
                },

                .expected = { details::DirectPlayResult{} },
            },

            // check container * is properly handled
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 1'000'000,
                    .maxTranscodingAudioBitrate = 320'000,
                    .directPlayProfiles = { {
                        { { "*" }, { "mp3" }, "*", std::nullopt },
                    } },
                    .transcodingProfiles = {},
                    .codecProfiles = {},
                },
                .containerInfo = { .bitrate = 128'000, .name = "mp3" },
                .audioStreamInfo = {
                    .index = 0,
                    .bitrate = 128'000,
                    .bitsPerSample = 16,
                    .channelCount = 2,
                    .sampleRate = 48'000,
                    .codec = av::DecodingCodec::MP3,
                    .codecName = "mp3",
                },

                .expected = { details::DirectPlayResult{} },
            },
        };

        processTests(testCases);
    }

} // namespace lms::api::subsonic
