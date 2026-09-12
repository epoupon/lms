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
#include <vector>

#include <gtest/gtest.h>

#include "core/Utils.hpp"
#include "core/media/Codec.hpp"
#include "core/media/Container.hpp"

#include "audio/ITranscoder.hpp"

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
            std::string name; // valid gtest identifier, used for the per-instantiation ctest name

            ClientInfo clientInfo;
            audio::AudioProperties source;

            detail::TranscodeDecisionResult expected;
        };

        std::ostream& operator<<(std::ostream& os, const TestCase& testCase)
        {
            return os << testCase.name;
        }

        class TranscodeDecisionTest : public ::testing::TestWithParam<TestCase>
        {
        };

        TEST_P(TranscodeDecisionTest, decides)
        {
            const TestCase& testCase{ GetParam() };

            if (const auto* transcodeResult{ std::get_if<detail::TranscodeResult>(&testCase.expected) })
            {
                if (const auto format{ detail::findAudioFormatByName(transcodeResult->targetStreamInfo.container, transcodeResult->targetStreamInfo.codec) })
                {
                    if (!audio::isEncodingSupported(format->container, format->codec))
                        GTEST_SKIP() << "transcoding target '" << transcodeResult->targetStreamInfo.container << "/" << transcodeResult->targetStreamInfo.codec << "' not supported by this build";
                }
            }

            EXPECT_EQ(testCase.expected, detail::computeTranscodeDecision(testCase.clientInfo, testCase.source));
        }

        std::vector<TestCase> buildDirectPlayTestCases()
        {
            return {
                // Direct play
                {
                    .name = "BasicDirectPlay",
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
                    .name = "WildcardProtocolAndCodec",
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
                    .name = "WildcardContainer",
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
                    .name = "Mp4AlacNoCodecRestriction",
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
                    .name = "Mp4AacExplicitCodecRestriction",
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

                // raw ADTS AAC (no real container of its own) direct-plays when the client declares "aac" container
                // support; "aac"/"adts" must resolve to the MPEG/AAC pair here, not to MP4 (which means a boxed .m4a/.mp4)
                {
                    .name = "RawAdtsAacDirectPlayMatchesAacContainerName",
                    .clientInfo = {
                        .name = "TestClient",
                        .platform = "TestPlatform",
                        .maxAudioBitrate = std::nullopt,
                        .maxTranscodingAudioBitrate = std::nullopt,
                        .directPlayProfiles = {
                            { .containers = { "aac" }, .audioCodecs = { "aac" }, .protocols = { "http" }, .maxAudioChannels = std::nullopt },
                        },
                        .transcodingProfiles = {},
                        .codecProfiles = {},
                    },
                    .source = {
                        .container = core::media::Container::MPEG,
                        .codec = core::media::Codec::AAC,
                        .duration = std::chrono::seconds{ 60 },
                        .bitrate = 128'000,
                        .channelCount = 2,
                        .sampleRate = 44'100,
                        .bitsPerSample = std::nullopt,
                    },

                    .expected = { detail::DirectPlayResult{} },
                },

                // direct play never needs the server to decode anything (the source bytes are streamed as-is),
                // so it must succeed even for a source format this build can't decode at all
                {
                    .name = "DirectPlayNotGatedByDecodingSupport",
                    .clientInfo = {
                        .name = "TestClient",
                        .platform = "TestPlatform",
                        .maxAudioBitrate = std::nullopt,
                        .maxTranscodingAudioBitrate = std::nullopt,
                        .directPlayProfiles = {
                            { .containers = { "wav" }, .audioCodecs = {}, .protocols = {}, .maxAudioChannels = std::nullopt },
                        },
                        .transcodingProfiles = {},
                        .codecProfiles = {},
                    },
                    .source = {
                        .container = core::media::Container::WAV,
                        .codec = core::media::Codec::MP3, // never a registered WAV/MP3 pair: guaranteed unsupported for decoding, on any build
                        .duration = std::chrono::seconds{ 60 },
                        .bitrate = 128'000,
                        .channelCount = 2,
                        .sampleRate = 44'100,
                        .bitsPerSample = std::nullopt,
                    },

                    .expected = { detail::DirectPlayResult{} },
                },
            };
        }

        INSTANTIATE_TEST_SUITE_P(directPlay, TranscodeDecisionTest, ::testing::ValuesIn(buildDirectPlayTestCases()),
                                 [](const ::testing::TestParamInfo<TestCase>& info) { return info.param.name; });

        std::vector<TestCase> buildBitrateLimitationTestCases()
        {
            return {
                // Needs transcode due to codec limitation
                {
                    .name = "CodecBitrateLimitation",
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
                    .name = "GlobalDirectPlayBitrateLimitation",
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
                    .name = "GlobalLimitationMoreRestrictiveThanCodec",
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
                    .name = "FlacBitrateTooHigh",
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
        }

        INSTANTIATE_TEST_SUITE_P(bitrateLimitation, TranscodeDecisionTest, ::testing::ValuesIn(buildBitrateLimitationTestCases()),
                                 [](const ::testing::TestParamInfo<TestCase>& info) { return info.param.name; });

        std::vector<TestCase> buildSampleRateLimitationTestCases()
        {
            return {
                // Needs transcode due to max audio sample rate not handle by codec limitation
                {
                    .name = "CodecSampleRateLimitation",
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
                    .name = "FlacSampleRateTooHigh",
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
                    .name = "FlacSampleRateTooHighNoMaxBitrate",
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
        }

        INSTANTIATE_TEST_SUITE_P(sampleRateLimitation, TranscodeDecisionTest, ::testing::ValuesIn(buildSampleRateLimitationTestCases()),
                                 [](const ::testing::TestParamInfo<TestCase>& info) { return info.param.name; });

        std::vector<TestCase> buildChannelLimitationTestCases()
        {
            return {
                // Needs transcode due to max nb channels not handle by profile
                {
                    .name = "ProfileChannelLimitation",
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
                    .name = "CodecChannelLimitation",
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
        }

        INSTANTIATE_TEST_SUITE_P(channelLimitation, TranscodeDecisionTest, ::testing::ValuesIn(buildChannelLimitationTestCases()),
                                 [](const ::testing::TestParamInfo<TestCase>& info) { return info.param.name; });

        std::vector<TestCase> buildCodecFallbackSelectionTestCases()
        {
            return {
                // needs transcode because codec not handled
                {
                    .name = "CodecNotHandled",
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
                    .name = "LosslessSourceUsesMaxBitrate",
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
                    .name = "LosslessSourceUsesDefaultBitrate",
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

                // a transcoding profile named "aac"/"aac" must resolve to the encodable MPEG/AAC (raw ADTS) pair,
                // not the MP4/AAC pair (muxing to MP4 is disabled: it requires a seekable output)
                {
                    .name = "AacTranscodingTargetResolvesToAdtsMuxer",
                    .clientInfo = { .name = "TestClient", .platform = "TestPlatform", .maxAudioBitrate = std::nullopt, .maxTranscodingAudioBitrate = std::nullopt, .directPlayProfiles = {
                                                                                                                                                                       { .containers = { "mp3" }, .audioCodecs = { "mp3" }, .protocols = { "http" }, .maxAudioChannels = std::nullopt },
                                                                                                                                                                   },
                                    .transcodingProfiles = {
                                        { .container = "aac", .audioCodec = "aac", .protocol = "http", .maxAudioChannels = std::nullopt },
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

                    .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::ContainerNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "aac", .codec = "aac", .audioChannels = std::nullopt, .audioBitrate = 256000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
                },

                // PCM (WAV) is a lossless codec: a WAV source must be able to fall back to a lossless transcoding target, just like FLAC
                {
                    .name = "PcmFallsBackToLosslessTarget",
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
                    .name = "PcmToLossyTargetUsesDefaultBitrate",
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
                    .name = "LossySourceFallsBackToLossyTarget",
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

                    // the "aac" transcoding profile (listed before "mp3") wins: it resolves to the encodable
                    // MPEG/AAC (raw ADTS) pair, not the disabled MP4/AAC muxer
                    .expected = { detail::TranscodeResult{ .reasons = { detail::TranscodeReason::ContainerNotSupported, detail::TranscodeReason::ContainerNotSupported, detail::TranscodeReason::ContainerNotSupported }, .targetStreamInfo = { .protocol = "http", .container = "aac", .codec = "aac", .audioChannels = std::nullopt, .audioBitrate = 128000, .audioProfile = "", .audioSamplerate = std::nullopt, .audioBitdepth = std::nullopt } } },
                },

                // wants a lossless codec not handled -> transcode to lossless
                {
                    .name = "LosslessSourceFallsBackToLosslessTarget",
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
        }

        INSTANTIATE_TEST_SUITE_P(codecFallbackSelection, TranscodeDecisionTest, ::testing::ValuesIn(buildCodecFallbackSelectionTestCases()),
                                 [](const ::testing::TestParamInfo<TestCase>& info) { return info.param.name; });

        std::vector<TestCase> buildProfileMatchingTestCases()
        {
            return {
                // no protocol specified
                {
                    .name = "NoProtocolSpecified",
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
                    .name = "DirectPlayCodecMismatch",
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
                    .name = "DirectPlayProtocolMismatch",
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
        }

        INSTANTIATE_TEST_SUITE_P(profileMatching, TranscodeDecisionTest, ::testing::ValuesIn(buildProfileMatchingTestCases()),
                                 [](const ::testing::TestParamInfo<TestCase>& info) { return info.param.name; });

        std::vector<TestCase> buildBitDepthTestCases()
        {
            return {
                // transcoding to a lossy codec must not report a bit depth, even if a bit depth limitation is set on that codec (lossy codecs have no PCM bit depth)
                {
                    .name = "LossyTargetOmitsBitDepth",
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
                    .name = "LosslessTargetReportsBitDepth",
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
                    .name = "BitDepthLimitationRejectsDirectPlay",
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
                    .name = "BitDepthLimitationAgainstLossySource",
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
        }

        INSTANTIATE_TEST_SUITE_P(bitDepth, TranscodeDecisionTest, ::testing::ValuesIn(buildBitDepthTestCases()),
                                 [](const ::testing::TestParamInfo<TestCase>& info) { return info.param.name; });

        std::vector<TestCase> buildOptionalLimitationTestCases()
        {
            return {
                // A non-required limitation is a preference only: it never blocks direct play, even when clearly violated (source has 6 channels vs the allowed 1 or 2)
                {
                    .name = "OptionalLimitationNeverBlocksDirectPlay",
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
                    .name = "OptionalLimitationShapesTranscodeTarget",
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
        }

        INSTANTIATE_TEST_SUITE_P(optionalLimitation, TranscodeDecisionTest, ::testing::ValuesIn(buildOptionalLimitationTestCases()),
                                 [](const ::testing::TestParamInfo<TestCase>& info) { return info.param.name; });

        std::vector<TestCase> buildComparisonOperatorsTestCases()
        {
            return {
                // Equals comparison (single value): mismatch cannot be adjusted -> rejects direct play, falls back to a transcoding profile not covered by that codec profile
                {
                    .name = "EqualsSingleValueMismatch",
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
                    .name = "EqualsMultipleValuesAdjustedDown",
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
                    .name = "NotEqualsForbiddenValue",
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
                    .name = "GreaterThanEqualBelowMinimum",
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
        }

        INSTANTIATE_TEST_SUITE_P(comparisonOperators, TranscodeDecisionTest, ::testing::ValuesIn(buildComparisonOperatorsTestCases()),
                                 [](const ::testing::TestParamInfo<TestCase>& info) { return info.param.name; });

        std::vector<TestCase> buildFailureTestCases()
        {
            return {
                // no compatible direct play or transcoding profile at all: transcoding profiles are skipped (unsupported output format, then non-http protocol) -> failure
                {
                    .name = "NoCompatibleProfile",
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

                // transcoding always needs the server to decode the source first: if this build can't decode
                // it at all, no transcoding profile could ever succeed, so we must fail fast with a precise
                // reason instead of iterating a transcoding-profile loop that could never work
                {
                    .name = "SourceDecodingNotSupported",
                    .clientInfo = {
                        .name = "TestClient",
                        .platform = "TestPlatform",
                        .maxAudioBitrate = std::nullopt,
                        .maxTranscodingAudioBitrate = std::nullopt,
                        .directPlayProfiles = {},
                        .transcodingProfiles = {
                            { .container = "mp3", .audioCodec = "mp3", .protocol = "http", .maxAudioChannels = std::nullopt },
                        },
                        .codecProfiles = {},
                    },
                    .source = {
                        .container = core::media::Container::WAV,
                        .codec = core::media::Codec::MP3, // never a registered WAV/MP3 pair: guaranteed unsupported for decoding, on any build
                        .duration = std::chrono::seconds{ 60 },
                        .bitrate = 128'000,
                        .channelCount = 2,
                        .sampleRate = 44'100,
                        .bitsPerSample = std::nullopt,
                    },

                    .expected = { detail::FailureResult{ "Source audio format is not supported for decoding by this server" } },
                },

                // MP4-boxed AAC must NOT match a client's "aac"-only container declaration: "aac" means raw ADTS,
                // distinct from the "m4a"/"mp4" names a client uses to declare support for boxed AAC
                {
                    .name = "Mp4AacDoesNotMatchAacOnlyContainerName",
                    .clientInfo = {
                        .name = "TestClient",
                        .platform = "TestPlatform",
                        .maxAudioBitrate = std::nullopt,
                        .maxTranscodingAudioBitrate = std::nullopt,
                        .directPlayProfiles = {
                            { .containers = { "aac" }, .audioCodecs = { "aac" }, .protocols = { "http" }, .maxAudioChannels = std::nullopt },
                        },
                        .transcodingProfiles = {},
                        .codecProfiles = {},
                    },
                    .source = {
                        .container = core::media::Container::MP4,
                        .codec = core::media::Codec::AAC,
                        .duration = std::chrono::seconds{ 60 },
                        .bitrate = 128'000,
                        .channelCount = 2,
                        .sampleRate = 44'100,
                        .bitsPerSample = std::nullopt,
                    },

                    .expected = { detail::FailureResult{ "No compatible direct play or transcoding profile found" } },
                },
            };
        }

        INSTANTIATE_TEST_SUITE_P(failure, TranscodeDecisionTest, ::testing::ValuesIn(buildFailureTestCases()),
                                 [](const ::testing::TestParamInfo<TestCase>& info) { return info.param.name; });

    } // namespace
} // namespace lms::api::subsonic
