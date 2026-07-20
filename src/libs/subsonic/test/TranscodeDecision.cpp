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
#include "core/media/Codec.hpp"
#include "core/media/Container.hpp"

#include "endpoints/transcoding/TranscodeDecision.hpp"
#include "payloads/ClientInfo.hpp"

namespace lms::api::subsonic
{
    namespace detail
    {
        std::ostream& operator<<(std::ostream& os, const detail::TranscodeDecisionResult& result)
        {
            std::visit(core::utils::overloads{
                           [&](const detail::DirectPlayResult&) { os << "direct play"; },
                           [&](const detail::FailureResult& res) { os << "failure: " << res.reason; },
                           [&](const detail::TranscodeResult& res) {
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
    }; // namespace detail

    namespace
    {
        struct TestCase
        {
            ClientInfo clientInfo;
            audio::AudioProperties source;

            detail::TranscodeDecisionResult expected;
        };

        void processTests(std::span<const TestCase> testCases)
        {
            for (std::size_t testCaseIndex{ 0 }; testCaseIndex < std::size(testCases); ++testCaseIndex)
            {
                const auto& testCase{ testCases[testCaseIndex] };
                const detail::TranscodeDecisionResult decision{ detail::computeTranscodeDecision(testCase.clientInfo, testCase.source) };

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
                    .directPlayProfiles = {
                        { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocols = { "http" }, .maxAudioChannels = 2 },
                    },
                    .transcodingProfiles = {
                        { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = 2 },
                    },
                    .codecProfiles = { { .type = "AudioCodec", .name = "mp3", .limitations = {
                                                                                  { .name = Limitation::Type::AudioBitrate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "256000" }, .required = true },
                                                                              } } },
                },
                .source = {
                    .container = core::media::Container::MPEG,
                    .codec = core::media::Codec::MP3,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 128'000,
                    .channelCount = 2,
                    .sampleRate = 44'100,
                    .bitsPerSample = std::nullopt,
                },

                .expected = { detail::DirectPlayResult{} },
            },

            // check protocol * and codec * are properly handled
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 1'000'000,
                    .maxTranscodingAudioBitrate = 320'000,
                    .directPlayProfiles = {
                        { .containers = { "mp4", "flac", "mp3" }, .audioCodecs = {}, .protocols = {}, .maxAudioChannels = std::nullopt },
                    },
                    .transcodingProfiles = {},
                    .codecProfiles = {},
                },
                .source = {
                    .container = core::media::Container::MPEG,
                    .codec = core::media::Codec::MP3,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 128'000,
                    .channelCount = 2,
                    .sampleRate = 48'000,
                    .bitsPerSample = std::nullopt,
                },

                .expected = { detail::DirectPlayResult{} },
            },

            // check container * is properly handled
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 1'000'000,
                    .maxTranscodingAudioBitrate = 320'000,
                    .directPlayProfiles = {
                        { .containers = {}, .audioCodecs = { "mp3" }, .protocols = {}, .maxAudioChannels = std::nullopt },
                    },
                    .transcodingProfiles = {},
                    .codecProfiles = {},
                },
                .source = {
                    .container = core::media::Container::MPEG,
                    .codec = core::media::Codec::MP3,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 128'000,
                    .channelCount = 2,
                    .sampleRate = 48'000,
                    .bitsPerSample = std::nullopt,
                },

                .expected = { detail::DirectPlayResult{} },
            },

            // MP4 container with ALAC (lossless) codec and no codec restriction direct-plays like any other supported container/codec pair
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = std::nullopt,
                    .maxTranscodingAudioBitrate = std::nullopt,
                    .directPlayProfiles = {
                        { .containers = { "m4a", "mp4" }, .audioCodecs = {}, .protocols = {}, .maxAudioChannels = std::nullopt },
                    },
                    .transcodingProfiles = {},
                    .codecProfiles = {},
                },
                .source = {
                    .container = core::media::Container::MP4,
                    .codec = core::media::Codec::ALAC,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 1'011'000,
                    .channelCount = 2,
                    .sampleRate = 44'100,
                    .bitsPerSample = 16,
                },

                .expected = { detail::DirectPlayResult{} },
            },

            // MP4 container with an explicit aac audioCodecs restriction (as declared by real AAC-only profiles) direct-plays when the codec matches
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = std::nullopt,
                    .maxTranscodingAudioBitrate = std::nullopt,
                    .directPlayProfiles = {
                        { .containers = { "m4a", "mp4" }, .audioCodecs = { "aac" }, .protocols = {}, .maxAudioChannels = std::nullopt },
                    },
                    .transcodingProfiles = {},
                    .codecProfiles = {},
                },
                .source = {
                    .container = core::media::Container::MP4,
                    .codec = core::media::Codec::AAC,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 257'000,
                    .channelCount = 2,
                    .sampleRate = 44'100,
                    .bitsPerSample = std::nullopt,
                },

                .expected = { detail::DirectPlayResult{} },
            },
        };

        processTests(testCases);
    }

    TEST(TranscodeDecision, bitrateLimitation)
    {
        const TestCase testCases[]{
            // Needs transcode due to codec limitation
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 512'000,
                    .maxTranscodingAudioBitrate = 96'000,
                    .directPlayProfiles = {
                        { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocols = { "http" }, .maxAudioChannels = 2 },
                    },
                    .transcodingProfiles = {
                        { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = 2 },
                    },
                    .codecProfiles = { { .type = "AudioCodec", .name = "mp3", .limitations = {
                                                                                  { .name = Limitation::Type::AudioBitrate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "96000" }, .required = true },
                                                                              } } },
                },
                .source = {
                    .container = core::media::Container::MPEG,
                    .codec = core::media::Codec::MP3,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 128'000,
                    .channelCount = 2,
                    .sampleRate = 44'100,
                    .bitsPerSample = std::nullopt,
                },

                .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::AudioBitrateNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "mp3", .codec = "mp3", .audioChannels = std::nullopt, .audioBitrate = 96000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },

            // Needs transcode due to global limitation on the direct play bitrate
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 96'000,
                    .maxTranscodingAudioBitrate = 96'000,
                    .directPlayProfiles = {
                        { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocols = { "http" }, .maxAudioChannels = 2 },
                    },
                    .transcodingProfiles = {
                        { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = 2 },
                    },
                    .codecProfiles = { { .type = "AudioCodec", .name = "mp3", .limitations = {
                                                                                  { .name = Limitation::Type::AudioBitrate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "256000" }, .required = true },
                                                                              } } },
                },
                .source = {
                    .container = core::media::Container::MPEG,
                    .codec = core::media::Codec::MP3,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 128'000,
                    .channelCount = 2,
                    .sampleRate = 44'100,
                    .bitsPerSample = std::nullopt,
                },

                .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::AudioBitrateNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "mp3", .codec = "mp3", .audioChannels = std::nullopt, .audioBitrate = 96000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },

            // Needs transcode due to codec limitation, but global limitation is even more restrictive
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 96'000,
                    .maxTranscodingAudioBitrate = 96'000,
                    .directPlayProfiles = {
                        { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocols = { "http" }, .maxAudioChannels = 2 },
                    },
                    .transcodingProfiles = {
                        { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = 2 },
                    },
                    .codecProfiles = { { .type = "AudioCodec", .name = "mp3", .limitations = {
                                                                                  { .name = Limitation::Type::AudioBitrate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "128000" }, .required = true },
                                                                              } } },
                },
                .source = {
                    .container = core::media::Container::MPEG,
                    .codec = core::media::Codec::MP3,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 192'000,
                    .channelCount = 2,
                    .sampleRate = 44'100,
                    .bitsPerSample = std::nullopt,
                },

                .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::AudioBitrateNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "mp3", .codec = "mp3", .audioChannels = std::nullopt, .audioBitrate = 96'000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },

            // want flac but bitrate too high
            {
                .clientInfo = {
                    .name = "LocalDevice",
                    .platform = "Android",
                    .maxAudioBitrate = 320'000,
                    .maxTranscodingAudioBitrate = 320'000,
                    .directPlayProfiles = {
                        { .containers = { "flac" }, .audioCodecs = { "flac" }, .protocols = {}, .maxAudioChannels = 32 },
                    },
                    .transcodingProfiles = { { .container = "ogg", .audioCodec = "opus", .protocol = "http", .maxAudioChannels = std::nullopt }, { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = 2 } },
                    .codecProfiles = {},
                },
                .source = {
                    .container = core::media::Container::FLAC,
                    .codec = core::media::Codec::FLAC,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 1'000'000,
                    .channelCount = 2,
                    .sampleRate = 48'000,
                    .bitsPerSample = 16,
                },

                .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::AudioBitrateNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "ogg", .codec = "opus", .audioChannels = std::nullopt, .audioBitrate = 320'000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },
        };

        processTests(testCases);
    }

    TEST(TranscodeDecision, sampleRateLimitation)
    {
        const TestCase testCases[]{
            // Needs transcode due to max audio sample rate not handle by codec limitation
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 320'000,
                    .maxTranscodingAudioBitrate = 320'000,
                    .directPlayProfiles = {
                        { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocols = { "http" }, .maxAudioChannels = 2 },
                    },
                    .transcodingProfiles = {
                        { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = 2 },
                    },
                    .codecProfiles = { { .type = "AudioCodec", .name = "mp3", .limitations = {
                                                                                  { .name = Limitation::Type::AudioSamplerate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "48000" }, .required = true },
                                                                              } } },
                },
                .source = {
                    .container = core::media::Container::MPEG,
                    .codec = core::media::Codec::MP3,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 192'000,
                    .channelCount = 2,
                    .sampleRate = 96'000,
                    .bitsPerSample = std::nullopt,
                },

                .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::AudioSampleRateNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "mp3", .codec = "mp3", .audioChannels = std::nullopt, .audioBitrate = 192'000, .audioProfile = "", .audioSamplerate = 48'000, .audioBitdepth = std::nullopt } } },
            },

            // want flac but source sample rate is too high
            {
                .clientInfo = {
                    .name = "SONOS",
                    .platform = "UPnP",
                    .maxAudioBitrate = 1'000'000,
                    .maxTranscodingAudioBitrate = 1'000'000,
                    .directPlayProfiles = {
                        { .containers = { "flac" }, .audioCodecs = {}, .protocols = {}, .maxAudioChannels = std::nullopt },
                        { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocols = {}, .maxAudioChannels = std::nullopt },
                        { .containers = { "m4a", "mp4" }, .audioCodecs = { "aac" }, .protocols = {}, .maxAudioChannels = std::nullopt },
                    },
                    .transcodingProfiles = {
                        { .container = "flac", .audioCodec = "flac", .protocol = "http", .maxAudioChannels = 6 },
                        { .container = "aac", .audioCodec = "aac", .protocol = "http", .maxAudioChannels = 6 },
                        { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = 2 },
                    },
                    .codecProfiles = {
                        { .type = "AudioCodec", .name = "flac", .limitations = { { .name = Limitation::Type::AudioSamplerate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "48000" }, .required = true } } },
                        { .type = "AudioCodec", .name = "vorbis", .limitations = { { .name = Limitation::Type::AudioSamplerate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "48000" }, .required = true } } },
                        { .type = "AudioCodec", .name = "opus", .limitations = { { .name = Limitation::Type::AudioSamplerate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "48000" }, .required = true } } },
                    },
                },
                .source = {
                    .container = core::media::Container::FLAC,
                    .codec = core::media::Codec::FLAC,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 950'000,
                    .channelCount = 2,
                    .sampleRate = 96'000,
                    .bitsPerSample = 24,
                },

                .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::AudioSampleRateNotSupported, detail::TranscodeReason::ContainerNotSupported, detail::TranscodeReason::ContainerNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "flac", .codec = "flac", .audioChannels = std::nullopt, .audioBitrate = std::nullopt, .audioProfile = "", .audioSamplerate = 48'000, .audioBitdepth = std::nullopt } } },
            },

            // want flac but source sample rate is too high, no max bitrate
            {
                .clientInfo = {
                    .name = "SONOS",
                    .platform = "UPnP",
                    .maxAudioBitrate = std::nullopt,
                    .maxTranscodingAudioBitrate = std::nullopt,
                    .directPlayProfiles = {
                        { .containers = { "opus", "ogg", "oga", "aac", "webma", "webm", "wav", "flac", "mka" }, .audioCodecs = {}, .protocols = {}, .maxAudioChannels = std::nullopt },
                        { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocols = {}, .maxAudioChannels = std::nullopt },
                        { .containers = { "m4a", "mp4" }, .audioCodecs = { "aac" }, .protocols = {}, .maxAudioChannels = std::nullopt },
                    },
                    .transcodingProfiles = {
                        { .container = "flac", .audioCodec = "flac", .protocol = "http", .maxAudioChannels = 6 },
                        { .container = "mp4", .audioCodec = "aac", .protocol = "http", .maxAudioChannels = 6 },
                        { .container = "aac", .audioCodec = "aac", .protocol = "http", .maxAudioChannels = 6 },
                        { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = 2 },
                    },
                    .codecProfiles = {
                        { .type = "AudioCodec", .name = "flac", .limitations = { { .name = Limitation::Type::AudioSamplerate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "48000" }, .required = true } } },
                        { .type = "AudioCodec", .name = "vorbis", .limitations = { { .name = Limitation::Type::AudioSamplerate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "48000" }, .required = true } } },
                        { .type = "AudioCodec", .name = "opus", .limitations = { { .name = Limitation::Type::AudioSamplerate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "48000" }, .required = true } } },
                    },
                },
                .source = {
                    .container = core::media::Container::FLAC,
                    .codec = core::media::Codec::FLAC,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 950'000,
                    .channelCount = 2,
                    .sampleRate = 96'000,
                    .bitsPerSample = 24,
                },

                .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::AudioSampleRateNotSupported, detail::TranscodeReason::ContainerNotSupported, detail::TranscodeReason::ContainerNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "flac", .codec = "flac", .audioChannels = std::nullopt, .audioBitrate = std::nullopt, .audioProfile = "", .audioSamplerate = 48'000, .audioBitdepth = std::nullopt } } },
            },
        };

        processTests(testCases);
    }

    TEST(TranscodeDecision, channelLimitation)
    {
        const TestCase testCases[]{
            // Needs transcode due to max nb channels not handle by profile
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 320'000,
                    .maxTranscodingAudioBitrate = 320'000,
                    .directPlayProfiles = { { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocols = { "http" }, .maxAudioChannels = 2 } },
                    .transcodingProfiles = { { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = 2 } },
                    .codecProfiles = { { .type = "AudioCodec", .name = "mp3", .limitations = {} } },
                },
                .source = {
                    .container = core::media::Container::MPEG,
                    .codec = core::media::Codec::MP3,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 192'000,
                    .channelCount = 5,
                    .sampleRate = 48'000,
                    .bitsPerSample = std::nullopt,
                },

                .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::AudioChannelsNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "mp3", .codec = "mp3", .audioChannels = 2, .audioBitrate = 192'000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },

            // Needs transcode due to max nb channels not handle by codec. TODO take channel reduction into account for bitrate
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 320'000,
                    .maxTranscodingAudioBitrate = 320'000,
                    .directPlayProfiles = {
                        { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocols = { "http" }, .maxAudioChannels = std::nullopt },
                    },
                    .transcodingProfiles = {
                        { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = std::nullopt },
                    },
                    .codecProfiles = { { .type = "AudioCodec", .name = "mp3", .limitations = { { .name = Limitation::Type::AudioChannels, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "2" }, .required = true } } } },
                },
                .source = {
                    .container = core::media::Container::MPEG,
                    .codec = core::media::Codec::MP3,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 192'000,
                    .channelCount = 5,
                    .sampleRate = 48'000,
                    .bitsPerSample = std::nullopt,
                },

                .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::AudioChannelsNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "mp3", .codec = "mp3", .audioChannels = 2, .audioBitrate = 192'000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },
        };

        processTests(testCases);
    }

    TEST(TranscodeDecision, codecFallbackSelection)
    {
        const TestCase testCases[]{
            // needs transcode because codec not handled
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 320'000,
                    .maxTranscodingAudioBitrate = 320'000,
                    .directPlayProfiles = {
                        { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocols = { "http" }, .maxAudioChannels = std::nullopt },
                    },
                    .transcodingProfiles = {
                        { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = std::nullopt },
                    },
                    .codecProfiles = { { .type = "AudioCodec", .name = "mp3", .limitations = { { .name = Limitation::Type::AudioChannels, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "2" }, .required = true } } } },
                },
                .source = {
                    .container = core::media::Container::Ogg,
                    .codec = core::media::Codec::Opus,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 128'000,
                    .channelCount = 2,
                    .sampleRate = 48'000,
                    .bitsPerSample = std::nullopt,
                },

                .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::ContainerNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "mp3", .codec = "mp3", .audioChannels = std::nullopt, .audioBitrate = 128'000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },

            // needs transcode because codec not handled (lossless source => using max bitrate)
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 1'000'000,
                    .maxTranscodingAudioBitrate = 320'000,
                    .directPlayProfiles = {
                        { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocols = { "http" }, .maxAudioChannels = std::nullopt },
                    },
                    .transcodingProfiles = {
                        { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = std::nullopt },
                    },
                    .codecProfiles = { { .type = "AudioCodec", .name = "mp3", .limitations = {} } },
                },
                .source = {
                    .container = core::media::Container::FLAC,
                    .codec = core::media::Codec::FLAC,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 750'000,
                    .channelCount = 2,
                    .sampleRate = 48'000,
                    .bitsPerSample = 16,
                },

                .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::ContainerNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "mp3", .codec = "mp3", .audioChannels = std::nullopt, .audioBitrate = 320000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },

            // needs transcode because codec not handled (lossless source => using a default good bitrate)
            {
                .clientInfo = { .name = "TestClient", .platform = "TestPlatform", .maxAudioBitrate = std::nullopt, .maxTranscodingAudioBitrate = std::nullopt, .directPlayProfiles = {
                                                                                                                                                                   { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocols = { "http" }, .maxAudioChannels = std::nullopt },
                                                                                                                                                               },
                                .transcodingProfiles = {
                                    { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = std::nullopt },
                                },
                                .codecProfiles = {} },
                .source = {
                    .container = core::media::Container::FLAC,
                    .codec = core::media::Codec::FLAC,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 750'000,
                    .channelCount = 2,
                    .sampleRate = 48'000,
                    .bitsPerSample = 16,
                },

                .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::ContainerNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "mp3", .codec = "mp3", .audioChannels = std::nullopt, .audioBitrate = 256000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },

            // PCM (WAV) is a lossless codec: a WAV source must be able to fall back to a lossless transcoding target, just like FLAC
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = std::nullopt,
                    .maxTranscodingAudioBitrate = std::nullopt,
                    .directPlayProfiles = {
                        { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocols = { "http" }, .maxAudioChannels = std::nullopt },
                    },
                    .transcodingProfiles = {
                        { .container = "flac", .audioCodec = "flac", .protocol = "http", .maxAudioChannels = std::nullopt },
                    },
                    .codecProfiles = {},
                },
                .source = {
                    .container = core::media::Container::WAV,
                    .codec = core::media::Codec::PCM,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 1'411'200,
                    .channelCount = 2,
                    .sampleRate = 44'100,
                    .bitsPerSample = 16,
                },

                .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::ContainerNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "flac", .codec = "flac", .audioChannels = std::nullopt, .audioBitrate = std::nullopt, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },

            // PCM (WAV) is lossless, so transcoding to a lossy target with no bitrate cap must pick a sane default bitrate, not the raw PCM bitrate
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = std::nullopt,
                    .maxTranscodingAudioBitrate = std::nullopt,
                    .directPlayProfiles = {
                        { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocols = { "http" }, .maxAudioChannels = std::nullopt },
                    },
                    .transcodingProfiles = {
                        { .container = "ogg", .audioCodec = "opus", .protocol = "http", .maxAudioChannels = std::nullopt },
                    },
                    .codecProfiles = {},
                },
                .source = {
                    .container = core::media::Container::WAV,
                    .codec = core::media::Codec::PCM,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 1'411'200,
                    .channelCount = 2,
                    .sampleRate = 44'100,
                    .bitsPerSample = 16,
                },

                .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::ContainerNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "ogg", .codec = "opus", .audioChannels = std::nullopt, .audioBitrate = 256000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },

            // wants a lossy codec not handled -> transcode to lossy
            {
                .clientInfo = {
                    .name = "SONOS",
                    .platform = "UPnP",
                    .maxAudioBitrate = 1'000'000,
                    .maxTranscodingAudioBitrate = 1'000'000,
                    .directPlayProfiles = {
                        { .containers = { "flac" }, .audioCodecs = {}, .protocols = {}, .maxAudioChannels = std::nullopt },
                        { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocols = {}, .maxAudioChannels = std::nullopt },
                        { .containers = { "m4a", "mp4" }, .audioCodecs = { "aac" }, .protocols = {}, .maxAudioChannels = std::nullopt },
                    },
                    .transcodingProfiles = {
                        { .container = "flac", .audioCodec = "flac", .protocol = "http", .maxAudioChannels = 6 },
                        { .container = "aac", .audioCodec = "aac", .protocol = "http", .maxAudioChannels = 6 },
                        { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = 2 },
                    },
                    .codecProfiles = {
                        { .type = "AudioCodec", .name = "flac", .limitations = { { .name = Limitation::Type::AudioSamplerate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "48000" }, .required = true } } },
                        { .type = "AudioCodec", .name = "vorbis", .limitations = { { .name = Limitation::Type::AudioSamplerate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "48000" }, .required = true } } },
                        { .type = "AudioCodec", .name = "opus", .limitations = { { .name = Limitation::Type::AudioSamplerate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "48000" }, .required = true } } },
                    },
                },
                .source = {
                    .container = core::media::Container::Ogg,
                    .codec = core::media::Codec::Vorbis,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 128'000,
                    .channelCount = 2,
                    .sampleRate = 48'000,
                    .bitsPerSample = 16,
                },

                .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::ContainerNotSupported, detail::TranscodeReason::ContainerNotSupported, detail::TranscodeReason::ContainerNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "mp3", .codec = "mp3", .audioChannels = std::nullopt, .audioBitrate = 128000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },

            // wants a lossless codec not handled -> transcode to lossless
            {
                .clientInfo = {
                    .name = "SONOS",
                    .platform = "UPnP",
                    .maxAudioBitrate = 1'000'000,
                    .maxTranscodingAudioBitrate = 1'000'000,
                    .directPlayProfiles = {
                        { .containers = { "flac" }, .audioCodecs = {}, .protocols = {}, .maxAudioChannels = std::nullopt },
                        { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocols = {}, .maxAudioChannels = std::nullopt },
                        { .containers = { "m4a", "mp4" }, .audioCodecs = { "aac" }, .protocols = {}, .maxAudioChannels = std::nullopt },
                    },
                    .transcodingProfiles = {
                        { .container = "flac", .audioCodec = "flac", .protocol = "http", .maxAudioChannels = 6 },
                        { .container = "aac", .audioCodec = "aac", .protocol = "http", .maxAudioChannels = 6 },
                        { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = 2 },
                    },
                    .codecProfiles = {
                        { .type = "AudioCodec", .name = "flac", .limitations = { { .name = Limitation::Type::AudioSamplerate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "48000" }, .required = true } } },
                        { .type = "AudioCodec", .name = "vorbis", .limitations = { { .name = Limitation::Type::AudioSamplerate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "48000" }, .required = true } } },
                        { .type = "AudioCodec", .name = "opus", .limitations = { { .name = Limitation::Type::AudioSamplerate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "48000" }, .required = true } } },
                    },
                },
                .source = {
                    .container = core::media::Container::DSF,
                    .codec = core::media::Codec::DSD,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 950'000,
                    .channelCount = 2,
                    .sampleRate = 96'000,
                    .bitsPerSample = 24,
                },

                .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::ContainerNotSupported, detail::TranscodeReason::ContainerNotSupported, detail::TranscodeReason::ContainerNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "flac", .codec = "flac", .audioChannels = std::nullopt, .audioBitrate = std::nullopt, .audioProfile = "", .audioSamplerate = 48'000, .audioBitdepth = std::nullopt } } },
            },
        };

        processTests(testCases);
    }

    TEST(TranscodeDecision, profileMatching)
    {
        const TestCase testCases[]{
            // no protocol specified
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 512'000,
                    .maxTranscodingAudioBitrate = 96'000,
                    .directPlayProfiles = {
                        { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocols = {}, .maxAudioChannels = 2 },
                    },
                    .transcodingProfiles = {
                        { .container = "mp3", .audioCodec = "mp3", .protocol = { "http" }, .maxAudioChannels = 2 },
                    },
                    .codecProfiles = { { .type = "AudioCodec", .name = "mp3", .limitations = {
                                                                                  { .name = Limitation::Type::AudioBitrate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "96000" }, .required = true },
                                                                              } } },
                },
                .source = {
                    .container = core::media::Container::MPEG,
                    .codec = core::media::Codec::MP3,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 128'000,
                    .channelCount = 2,
                    .sampleRate = 44'100,
                    .bitsPerSample = std::nullopt,
                },

                .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::AudioBitrateNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "mp3", .codec = "mp3", .audioChannels = std::nullopt, .audioBitrate = 96000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },

            // needs transcode because audio codec not supported by the direct play profile (container matches, codec does not)
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 320'000,
                    .maxTranscodingAudioBitrate = 320'000,
                    .directPlayProfiles = {
                        { .containers = { "mp3" }, .audioCodecs = { "aac" }, .protocols = { "http" }, .maxAudioChannels = 2 },
                    },
                    .transcodingProfiles = {
                        { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = 2 },
                    },
                    .codecProfiles = {},
                },
                .source = {
                    .container = core::media::Container::MPEG,
                    .codec = core::media::Codec::MP3,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 128'000,
                    .channelCount = 2,
                    .sampleRate = 44'100,
                    .bitsPerSample = std::nullopt,
                },

                .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::AudioCodecNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "mp3", .codec = "mp3", .audioChannels = std::nullopt, .audioBitrate = 128'000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },

            // needs transcode because the direct play profile does not support the http protocol
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 320'000,
                    .maxTranscodingAudioBitrate = 320'000,
                    .directPlayProfiles = {
                        { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocols = { "hls" }, .maxAudioChannels = 2 },
                    },
                    .transcodingProfiles = {
                        { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = 2 },
                    },
                    .codecProfiles = {},
                },
                .source = {
                    .container = core::media::Container::MPEG,
                    .codec = core::media::Codec::MP3,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 128'000,
                    .channelCount = 2,
                    .sampleRate = 44'100,
                    .bitsPerSample = std::nullopt,
                },

                .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::ProtocolNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "mp3", .codec = "mp3", .audioChannels = std::nullopt, .audioBitrate = 128'000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },
        };

        processTests(testCases);
    }

    TEST(TranscodeDecision, bitDepth)
    {
        const TestCase testCases[]{
            // transcoding to a lossy codec must not report a bit depth, even if a bit depth limitation is set on that codec (lossy codecs have no PCM bit depth)
            {
                .clientInfo = { .name = "TestClient", .platform = "TestPlatform", .maxAudioBitrate = std::nullopt, .maxTranscodingAudioBitrate = std::nullopt, .directPlayProfiles = { {
                                                                                                                                                                   { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocols = { "http" }, .maxAudioChannels = std::nullopt },
                                                                                                                                                               } },
                                .transcodingProfiles = {
                                    { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = std::nullopt },
                                },
                                .codecProfiles = { { .type = "AudioCodec", .name = "mp3", .limitations = {
                                                                                              { .name = Limitation::Type::AudioBitdepth, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "16" }, .required = true },
                                                                                          } } } },
                .source = {
                    .container = core::media::Container::FLAC,
                    .codec = core::media::Codec::FLAC,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 750'000,
                    .channelCount = 2,
                    .sampleRate = 48'000,
                    .bitsPerSample = 24,
                },

                .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::ContainerNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "mp3", .codec = "mp3", .audioChannels = std::nullopt, .audioBitrate = 256000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },

            // transcoding to a lossless codec still reports the adjusted bit depth
            {
                .clientInfo = { .name = "TestClient", .platform = "TestPlatform", .maxAudioBitrate = std::nullopt, .maxTranscodingAudioBitrate = std::nullopt, .directPlayProfiles = { {
                                                                                                                                                                   { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocols = { "http" }, .maxAudioChannels = std::nullopt },
                                                                                                                                                               } },
                                .transcodingProfiles = {
                                    { .container = "flac", .audioCodec = "flac", .protocol = "http", .maxAudioChannels = std::nullopt },
                                },
                                .codecProfiles = { { .type = "AudioCodec", .name = "flac", .limitations = {
                                                                                               { .name = Limitation::Type::AudioBitdepth, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "16" }, .required = true },
                                                                                           } } } },
                .source = {
                    .container = core::media::Container::FLAC,
                    .codec = core::media::Codec::FLAC,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 750'000,
                    .channelCount = 2,
                    .sampleRate = 48'000,
                    .bitsPerSample = 24,
                },

                .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::ContainerNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "flac", .codec = "flac", .audioChannels = std::nullopt, .audioBitrate = std::nullopt, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = 16 } } },
            },

            // needs transcode because the codec profile bit depth limitation rejects direct play
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 1'000'000,
                    .maxTranscodingAudioBitrate = 1'000'000,
                    .directPlayProfiles = {
                        { .containers = { "flac" }, .audioCodecs = { "flac" }, .protocols = { "http" }, .maxAudioChannels = std::nullopt },
                    },
                    .transcodingProfiles = {
                        { .container = "flac", .audioCodec = "flac", .protocol = "http", .maxAudioChannels = std::nullopt },
                    },
                    .codecProfiles = { { .type = "AudioCodec", .name = "flac", .limitations = {
                                                                                   { .name = Limitation::Type::AudioBitdepth, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "16" }, .required = true },
                                                                               } } },
                },
                .source = {
                    .container = core::media::Container::FLAC,
                    .codec = core::media::Codec::FLAC,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 750'000,
                    .channelCount = 2,
                    .sampleRate = 48'000,
                    .bitsPerSample = 24,
                },

                .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::AudioBitdepthNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "flac", .codec = "flac", .audioChannels = std::nullopt, .audioBitrate = std::nullopt, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = 16 } } },
            },

            // a required bit depth limitation cannot be evaluated against a lossy source (no bit depth to check), so it is always treated as incompatible on both the direct play and transcoding target sides
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 1'000'000,
                    .maxTranscodingAudioBitrate = 1'000'000,
                    .directPlayProfiles = {
                        { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocols = { "http" }, .maxAudioChannels = std::nullopt },
                    },
                    .transcodingProfiles = {
                        { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = std::nullopt },
                        { .container = "ogg", .audioCodec = "opus", .protocol = "http", .maxAudioChannels = std::nullopt },
                    },
                    .codecProfiles = { { .type = "AudioCodec", .name = "mp3", .limitations = {
                                                                                  { .name = Limitation::Type::AudioBitdepth, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "24" }, .required = true },
                                                                              } } },
                },
                .source = {
                    .container = core::media::Container::MPEG,
                    .codec = core::media::Codec::MP3,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 128'000,
                    .channelCount = 2,
                    .sampleRate = 44'100,
                    .bitsPerSample = std::nullopt,
                },

                .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::AudioBitdepthNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "ogg", .codec = "opus", .audioChannels = std::nullopt, .audioBitrate = 128'000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },
        };

        processTests(testCases);
    }

    TEST(TranscodeDecision, optionalLimitation)
    {
        const TestCase testCases[]{
            // A non-required limitation is a preference only: it never blocks direct play, even when clearly violated (source has 6 channels vs the allowed 1 or 2)
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 1'000'000,
                    .maxTranscodingAudioBitrate = 1'000'000,
                    .directPlayProfiles = {
                        { .containers = { "flac" }, .audioCodecs = { "flac" }, .protocols = { "http" }, .maxAudioChannels = std::nullopt },
                    },
                    .transcodingProfiles = {
                        { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = 2 },
                    },
                    .codecProfiles = { { .type = "AudioCodec", .name = "flac", .limitations = {
                                                                                   { .name = Limitation::Type::AudioSamplerate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "192000" }, .required = false },
                                                                                   { .name = Limitation::Type::AudioChannels, .comparison = Limitation::ComparisonOperator::Equals, .values = { "1", "2" }, .required = false },
                                                                               } } },
                },
                .source = {
                    .container = core::media::Container::FLAC,
                    .codec = core::media::Codec::FLAC,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 900'000,
                    .channelCount = 6,
                    .sampleRate = 96'000,
                    .bitsPerSample = 24,
                },

                .expected = { detail::DirectPlayResult{} },
            },

            // A non-required limitation still shapes the transcoded target once a transcode is already happening for an unrelated reason (container mismatch): applyLimitation never checks `required`
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 1'000'000,
                    .maxTranscodingAudioBitrate = 1'000'000,
                    .directPlayProfiles = {
                        { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocols = { "http" }, .maxAudioChannels = std::nullopt },
                    },
                    .transcodingProfiles = {
                        { .container = "flac", .audioCodec = "flac", .protocol = "http", .maxAudioChannels = std::nullopt },
                    },
                    .codecProfiles = { { .type = "AudioCodec", .name = "flac", .limitations = {
                                                                                   { .name = Limitation::Type::AudioSamplerate, .comparison = Limitation::ComparisonOperator::LessThanEqual, .values = { "192000" }, .required = false },
                                                                                   { .name = Limitation::Type::AudioChannels, .comparison = Limitation::ComparisonOperator::Equals, .values = { "1", "2" }, .required = false },
                                                                               } } },
                },
                .source = {
                    .container = core::media::Container::FLAC,
                    .codec = core::media::Codec::FLAC,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 900'000,
                    .channelCount = 6,
                    .sampleRate = 96'000,
                    .bitsPerSample = 16,
                },

                .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::ContainerNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "flac", .codec = "flac", .audioChannels = 2, .audioBitrate = std::nullopt, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },
        };

        processTests(testCases);
    }

    TEST(TranscodeDecision, comparisonOperators)
    {
        const TestCase testCases[]{
            // Equals comparison (single value): mismatch cannot be adjusted -> rejects direct play, falls back to a transcoding profile not covered by that codec profile
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 1'000'000,
                    .maxTranscodingAudioBitrate = 1'000'000,
                    .directPlayProfiles = {
                        { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocols = { "http" }, .maxAudioChannels = std::nullopt },
                    },
                    .transcodingProfiles = {
                        { .container = "ogg", .audioCodec = "opus", .protocol = "http", .maxAudioChannels = std::nullopt },
                    },
                    .codecProfiles = { { .type = "AudioCodec", .name = "mp3", .limitations = {
                                                                                  { .name = Limitation::Type::AudioSamplerate, .comparison = Limitation::ComparisonOperator::Equals, .values = { "44100" }, .required = true },
                                                                              } } },
                },
                .source = {
                    .container = core::media::Container::MPEG,
                    .codec = core::media::Codec::MP3,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 128'000,
                    .channelCount = 2,
                    .sampleRate = 48'000,
                    .bitsPerSample = std::nullopt,
                },

                .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::AudioSampleRateNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "ogg", .codec = "opus", .audioChannels = std::nullopt, .audioBitrate = 128'000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },

            // Equals comparison (multiple values): no exact match -> adjusted down to the closest allowed value below the source's
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 1'000'000,
                    .maxTranscodingAudioBitrate = 1'000'000,
                    .directPlayProfiles = {
                        { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocols = { "http" }, .maxAudioChannels = std::nullopt },
                    },
                    .transcodingProfiles = {
                        { .container = "flac", .audioCodec = "flac", .protocol = "http", .maxAudioChannels = std::nullopt },
                    },
                    .codecProfiles = { { .type = "AudioCodec", .name = "flac", .limitations = {
                                                                                   { .name = Limitation::Type::AudioSamplerate, .comparison = Limitation::ComparisonOperator::Equals, .values = { "44100", "48000", "96000" }, .required = true },
                                                                               } } },
                },
                .source = {
                    .container = core::media::Container::FLAC,
                    .codec = core::media::Codec::FLAC,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 750'000,
                    .channelCount = 2,
                    .sampleRate = 60'000,
                    .bitsPerSample = 16,
                },

                .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::ContainerNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "flac", .codec = "flac", .audioChannels = std::nullopt, .audioBitrate = std::nullopt, .audioProfile = "", .audioSamplerate = 48'000, .audioBitdepth = std::nullopt } } },
            },

            // NotEquals comparison: source value is in the forbidden list and cannot be adjusted -> rejects direct play, falls back to a transcoding profile not covered by that codec profile
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 1'000'000,
                    .maxTranscodingAudioBitrate = 1'000'000,
                    .directPlayProfiles = {
                        { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocols = { "http" }, .maxAudioChannels = std::nullopt },
                    },
                    .transcodingProfiles = {
                        { .container = "ogg", .audioCodec = "opus", .protocol = "http", .maxAudioChannels = std::nullopt },
                    },
                    .codecProfiles = { { .type = "AudioCodec", .name = "mp3", .limitations = {
                                                                                  { .name = Limitation::Type::AudioChannels, .comparison = Limitation::ComparisonOperator::NotEquals, .values = { "2" }, .required = true },
                                                                              } } },
                },
                .source = {
                    .container = core::media::Container::MPEG,
                    .codec = core::media::Codec::MP3,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 128'000,
                    .channelCount = 2,
                    .sampleRate = 44'100,
                    .bitsPerSample = std::nullopt,
                },

                .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::AudioChannelsNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "ogg", .codec = "opus", .audioChannels = std::nullopt, .audioBitrate = 128'000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },

            // GreaterThanEqual comparison: source bitrate is below the required minimum and cannot be upscaled -> that transcoding profile is rejected entirely, falls back to the next one
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 1'000'000,
                    .maxTranscodingAudioBitrate = 1'000'000,
                    .directPlayProfiles = {
                        { .containers = { "flac" }, .audioCodecs = { "flac" }, .protocols = { "http" }, .maxAudioChannels = std::nullopt },
                    },
                    .transcodingProfiles = {
                        { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = std::nullopt },
                        { .container = "ogg", .audioCodec = "opus", .protocol = "http", .maxAudioChannels = std::nullopt },
                    },
                    .codecProfiles = { { .type = "AudioCodec", .name = "mp3", .limitations = {
                                                                                  { .name = Limitation::Type::AudioBitrate, .comparison = Limitation::ComparisonOperator::GreaterThanEqual, .values = { "192000" }, .required = true },
                                                                              } } },
                },
                .source = {
                    .container = core::media::Container::MPEG,
                    .codec = core::media::Codec::MP3,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 128'000,
                    .channelCount = 2,
                    .sampleRate = 44'100,
                    .bitsPerSample = std::nullopt,
                },

                .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::ContainerNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "ogg", .codec = "opus", .audioChannels = std::nullopt, .audioBitrate = 128'000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
            },
        };

        processTests(testCases);
    }

    TEST(TranscodeDecision, failure)
    {
        const TestCase testCases[]{
            // no compatible direct play or transcoding profile at all: transcoding profiles are skipped (unsupported output format, then non-http protocol) -> failure
            {
                .clientInfo = {
                    .name = "TestClient",
                    .platform = "TestPlatform",
                    .maxAudioBitrate = 320'000,
                    .maxTranscodingAudioBitrate = 320'000,
                    .directPlayProfiles = {
                        { .containers = { "flac" }, .audioCodecs = { "flac" }, .protocols = { "http" }, .maxAudioChannels = std::nullopt },
                    },
                    .transcodingProfiles = {
                        { .container = "wma", .audioCodec = "wma", .protocol = "http", .maxAudioChannels = std::nullopt },
                        { .container = "mp3", .audioCodec = "mp3", .protocol = "hls", .maxAudioChannels = std::nullopt },
                    },
                    .codecProfiles = {},
                },
                .source = {
                    .container = core::media::Container::MPEG,
                    .codec = core::media::Codec::MP3,
                    .duration = std::chrono::seconds{ 60 },
                    .bitrate = 128'000,
                    .channelCount = 2,
                    .sampleRate = 44'100,
                    .bitsPerSample = std::nullopt,
                },

                .expected = { detail::FailureResult{ "No compatible direct play or transcoding profile found" } },
            },
        };

        processTests(testCases);
    }
} // namespace lms::api::subsonic
