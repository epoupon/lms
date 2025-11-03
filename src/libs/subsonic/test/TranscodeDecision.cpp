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

#include <variant>

#include <gtest/gtest.h>

#include "core/Utils.hpp"

#include "audio/AudioTypes.hpp"

#include "endpoints/transcoding/TranscodeDecision.hpp"
#include "responses/ClientInfo.hpp"

namespace lms::api::subsonic
{
    namespace details
    {

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
                               os << "}, target stream = {";
                               os << "protocol = " << res.targetStreamInfo.protocol << ", container = " << res.targetStreamInfo.container << ", codec = " << res.targetStreamInfo.codec;
                               if (res.targetStreamInfo.audioChannels)
                                   os << ", audioChannels = " << *res.targetStreamInfo.audioChannels;
                               if (res.targetStreamInfo.audioBitrate)
                                   os << ", audioBitrate = " << *res.targetStreamInfo.audioBitrate;
                               if (!res.targetStreamInfo.audioProfile.empty())
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
            audio::AudioProperties source;

            details::TranscodeDecisionResult expected;
        };

        void processTests(std::span<const TestCase> testCases)
        {
            for (std::size_t testCaseIndex{ 0 }; testCaseIndex < std::size(testCases); ++testCaseIndex)
            {
                const auto& testCase{ testCases[testCaseIndex] };
                const details::TranscodeDecisionResult decision{ details::computeTranscodeDecision(testCase.clientInfo, testCase.source) };

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
                .source = {
                    .container = audio::ContainerType::MPEG,
                    .codec = audio::CodecType::MP3,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 128'000,
                    .channelCount = 2,
                    .sampleRate = 44'100,
                    .bitsPerSample = std::nullopt,
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
                .source = {
                    .container = audio::ContainerType::MPEG,
                    .codec = audio::CodecType::MP3,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 128'000,
                    .channelCount = 2,
                    .sampleRate = 44'100,
                    .bitsPerSample = std::nullopt,
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
                .source = {
                    .container = audio::ContainerType::MPEG,
                    .codec = audio::CodecType::MP3,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 128'000,
                    .channelCount = 2,
                    .sampleRate = 44'100,
                    .bitsPerSample = std::nullopt,
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
                .source = {
                    .container = audio::ContainerType::MPEG,
                    .codec = audio::CodecType::MP3,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 192'000,
                    .channelCount = 2,
                    .sampleRate = 44'100,
                    .bitsPerSample = std::nullopt,
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
                .source = {
                    .container = audio::ContainerType::MPEG,
                    .codec = audio::CodecType::MP3,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 192'000,
                    .channelCount = 2,
                    .sampleRate = 96'000,
                    .bitsPerSample = std::nullopt,
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
                .source = {
                    .container = audio::ContainerType::MPEG,
                    .codec = audio::CodecType::MP3,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 192'000,
                    .channelCount = 5,
                    .sampleRate = 48'000,
                    .bitsPerSample = std::nullopt,
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
                .source = {
                    .container = audio::ContainerType::MPEG,
                    .codec = audio::CodecType::MP3,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 192'000,
                    .channelCount = 5,
                    .sampleRate = 48'000,
                    .bitsPerSample = std::nullopt,
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
                .source = {
                    .container = audio::ContainerType::Ogg,
                    .codec = audio::CodecType::Opus,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 128'000,
                    .channelCount = 2,
                    .sampleRate = 48'000,
                    .bitsPerSample = std::nullopt,
                },

                .expected = { details::TranscodeResult{ .reasons = { details::TranscodeReason::ContainerNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "mp3", .codec = "mp3", .audioChannels = std::nullopt, .audioBitrate = 128'000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
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
                .source = {
                    .container = audio::ContainerType::FLAC,
                    .codec = audio::CodecType::FLAC,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 750'000,
                    .channelCount = 2,
                    .sampleRate = 48'000,
                    .bitsPerSample = 16,
                },

                .expected = { details::TranscodeResult{ .reasons = { details::TranscodeReason::ContainerNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "mp3", .codec = "mp3", .audioChannels = std::nullopt, .audioBitrate = 320000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
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
                .source = {
                    .container = audio::ContainerType::FLAC,
                    .codec = audio::CodecType::FLAC,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 750'000,
                    .channelCount = 2,
                    .sampleRate = 48'000,
                    .bitsPerSample = 16,
                },

                .expected = { details::TranscodeResult{ .reasons = { details::TranscodeReason::ContainerNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "mp3", .codec = "mp3", .audioChannels = std::nullopt, .audioBitrate = 256000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
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
                .source = {
                    .container = audio::ContainerType::MPEG,
                    .codec = audio::CodecType::MP3,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 128'000,
                    .channelCount = 2,
                    .sampleRate = 48'000,
                    .bitsPerSample = std::nullopt,
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
                .source = {
                    .container = audio::ContainerType::MPEG,
                    .codec = audio::CodecType::MP3,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 128'000,
                    .channelCount = 2,
                    .sampleRate = 48'000,
                    .bitsPerSample = std::nullopt,
                },

                .expected = { details::DirectPlayResult{} },
            },

            // want flac but bitrate too high
            {
                .clientInfo = {
                    .name = "LocalDevice",
                    .platform = "Android",
                    .maxAudioBitrate = 320'000,
                    .maxTranscodingAudioBitrate = 320'000,
                    .directPlayProfiles = { {
                        { .containers = { "flac" }, .audioCodecs = { "flac" }, .protocol = "*", .maxAudioChannels = 32 },
                    } },
                    .transcodingProfiles = { { { .container = "ogg", .audioCodec = "opus", .protocol = "http", .maxAudioChannels = std::nullopt }, { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = 2 } } },
                    .codecProfiles = {},
                },
                .source = {
                    .container = audio::ContainerType::FLAC,
                    .codec = audio::CodecType::FLAC,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 1'000'000,
                    .channelCount = 2,
                    .sampleRate = 48'000,
                    .bitsPerSample = 16,
                },

                .expected = { details::TranscodeResult{ .reasons = { details::TranscodeReason::AudioBitrateNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "ogg", .codec = "opus", .audioChannels = std::nullopt, .audioBitrate = 320'000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },

            // want flac but source sample rate is too high
            {
                .clientInfo = {
                    .name = "SONOS",
                    .platform = "UPnP",
                    .maxAudioBitrate = 1'000'000,
                    .maxTranscodingAudioBitrate = 1'000'000,
                    .directPlayProfiles = { {
                        { .containers = { "flac" }, .audioCodecs = { "*" }, .protocol = "*", .maxAudioChannels = std::nullopt },
                        { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocol = "*", .maxAudioChannels = std::nullopt },
                        { .containers = { "m4a", "mp4" }, .audioCodecs = { "aac" }, .protocol = "*", .maxAudioChannels = std::nullopt },
                    } },
                    .transcodingProfiles = { {
                        { .container = "flac", .audioCodec = "flac", .protocol = "http", .maxAudioChannels = 6 },
                        { .container = "aac", .audioCodec = "aac", .protocol = "http", .maxAudioChannels = 6 },
                        { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = 2 },

                    } },
                    .codecProfiles = {
                        {
                            { .type = "AudioCodec", .name = "flac", .limitations = { { .name = Limitation::Type::AudioSamplerate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "48000" }, .required = true } } },
                            { .type = "AudioCodec", .name = "vorbis", .limitations = { { .name = Limitation::Type::AudioSamplerate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "48000" }, .required = true } } },
                            { .type = "AudioCodec", .name = "opus", .limitations = { { .name = Limitation::Type::AudioSamplerate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "48000" }, .required = true } } },
                        },
                    },
                },
                .source = {
                    .container = audio::ContainerType::FLAC,
                    .codec = audio::CodecType::FLAC,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 950'000,
                    .channelCount = 2,
                    .sampleRate = 96'000,
                    .bitsPerSample = 24,
                },

                .expected = { details::TranscodeResult{ .reasons = { details::TranscodeReason::AudioSampleRateNotSupported, details::TranscodeReason::ContainerNotSupported, details::TranscodeReason::ContainerNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "flac", .codec = "flac", .audioChannels = std::nullopt, .audioBitrate = std::nullopt, .audioProfile = "", .audioSamplerate = 48'000, .audioBitdepth = std::nullopt } } },
            },

            // wants a lossy codec not handled -> transcode to lossy
            {
                .clientInfo = {
                    .name = "SONOS",
                    .platform = "UPnP",
                    .maxAudioBitrate = 1'000'000,
                    .maxTranscodingAudioBitrate = 1'000'000,
                    .directPlayProfiles = { {
                        { .containers = { "flac" }, .audioCodecs = { "*" }, .protocol = "*", .maxAudioChannels = std::nullopt },
                        { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocol = "*", .maxAudioChannels = std::nullopt },
                        { .containers = { "m4a", "mp4" }, .audioCodecs = { "aac" }, .protocol = "*", .maxAudioChannels = std::nullopt },
                    } },
                    .transcodingProfiles = { {
                        { .container = "flac", .audioCodec = "flac", .protocol = "http", .maxAudioChannels = 6 },
                        { .container = "aac", .audioCodec = "aac", .protocol = "http", .maxAudioChannels = 6 },
                        { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = 2 },

                    } },
                    .codecProfiles = {
                        {
                            { .type = "AudioCodec", .name = "flac", .limitations = { { .name = Limitation::Type::AudioSamplerate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "48000" }, .required = true } } },
                            { .type = "AudioCodec", .name = "vorbis", .limitations = { { .name = Limitation::Type::AudioSamplerate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "48000" }, .required = true } } },
                            { .type = "AudioCodec", .name = "opus", .limitations = { { .name = Limitation::Type::AudioSamplerate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "48000" }, .required = true } } },
                        },
                    },
                },
                .source = {
                    .container = audio::ContainerType::Ogg,
                    .codec = audio::CodecType::Vorbis,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 128'000,
                    .channelCount = 2,
                    .sampleRate = 48'000,
                    .bitsPerSample = 16,
                },

                .expected = { details::TranscodeResult{ .reasons = { details::TranscodeReason::ContainerNotSupported, details::TranscodeReason::ContainerNotSupported, details::TranscodeReason::ContainerNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "mp3", .codec = "mp3", .audioChannels = std::nullopt, .audioBitrate = 128000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },

            // wants a lossless codec not handled -> transcode to lossless
            {
                .clientInfo = {
                    .name = "SONOS",
                    .platform = "UPnP",
                    .maxAudioBitrate = 1'000'000,
                    .maxTranscodingAudioBitrate = 1'000'000,
                    .directPlayProfiles = { {
                        { .containers = { "flac" }, .audioCodecs = { "*" }, .protocol = "*", .maxAudioChannels = std::nullopt },
                        { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocol = "*", .maxAudioChannels = std::nullopt },
                        { .containers = { "m4a", "mp4" }, .audioCodecs = { "aac" }, .protocol = "*", .maxAudioChannels = std::nullopt },
                    } },
                    .transcodingProfiles = { {
                        { .container = "flac", .audioCodec = "flac", .protocol = "http", .maxAudioChannels = 6 },
                        { .container = "aac", .audioCodec = "aac", .protocol = "http", .maxAudioChannels = 6 },
                        { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = 2 },

                    } },
                    .codecProfiles = {
                        {
                            { .type = "AudioCodec", .name = "flac", .limitations = { { .name = Limitation::Type::AudioSamplerate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "48000" }, .required = true } } },
                            { .type = "AudioCodec", .name = "vorbis", .limitations = { { .name = Limitation::Type::AudioSamplerate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "48000" }, .required = true } } },
                            { .type = "AudioCodec", .name = "opus", .limitations = { { .name = Limitation::Type::AudioSamplerate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "48000" }, .required = true } } },
                        },
                    },
                },
                .source = {
                    .container = audio::ContainerType::DSF,
                    .codec = audio::CodecType::DSD,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 950'000,
                    .channelCount = 2,
                    .sampleRate = 96'000,
                    .bitsPerSample = 24,
                },

                .expected = { details::TranscodeResult{ .reasons = { details::TranscodeReason::ContainerNotSupported, details::TranscodeReason::ContainerNotSupported, details::TranscodeReason::ContainerNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "flac", .codec = "flac", .audioChannels = std::nullopt, .audioBitrate = std::nullopt, .audioProfile = "", .audioSamplerate = 48'000, .audioBitdepth = std::nullopt } } },
            },

            // * in protocol
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 512'000,
                    .maxTranscodingAudioBitrate = 96'000,
                    .directPlayProfiles = { {
                        { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocol = "*", .maxAudioChannels = 2 },
                    } },
                    .transcodingProfiles = {
                        { .container = "mp3", .audioCodec = "mp3", .protocol = "*", .maxAudioChannels = 2 },
                    },
                    .codecProfiles = { { .type = "AudioCodec", .name = "mp3", .limitations = {
                                                                                  { .name = Limitation::Type::AudioBitrate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "96000" }, .required = true },
                                                                              } } },
                },
                .source = {
                    .container = audio::ContainerType::MPEG,
                    .codec = audio::CodecType::MP3,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 128'000,
                    .channelCount = 2,
                    .sampleRate = 44'100,
                    .bitsPerSample = std::nullopt,
                },

                .expected = { details::TranscodeResult{ .reasons = { details::TranscodeReason::AudioBitrateNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "mp3", .codec = "mp3", .audioChannels = std::nullopt, .audioBitrate = 96000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },
        };

        processTests(testCases);
    }
} // namespace lms::api::subsonic
