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

#include "TranscodeDecision.hpp"

#include <array>

#include "core/ILogger.hpp"
#include "core/String.hpp"

#include "audio/TranscodeTypes.hpp"

#include "SubsonicResponse.hpp"
#include "payloads/ClientInfo.hpp"

namespace lms::api::subsonic::details
{
    namespace
    {
        constexpr std::array supportedTranscodeOutputFormats{
            audio::TranscodeOutputFormat{ .container = core::media::ContainerType::MPEG, .codec = core::media::CodecType::MP3 },
            audio::TranscodeOutputFormat{ .container = core::media::ContainerType::Ogg, .codec = core::media::CodecType::Vorbis },
            audio::TranscodeOutputFormat{ .container = core::media::ContainerType::Ogg, .codec = core::media::CodecType::Opus },
            audio::TranscodeOutputFormat{ .container = core::media::ContainerType::FLAC, .codec = core::media::CodecType::FLAC },
        };

        bool isMatchingContainerName(core::media::ContainerType container, std::string_view containerStr)
        {
            using namespace std::literals; // for "..."sv

            constexpr std::array aiffNames{ "aif"sv, "aiff"sv };
            constexpr std::array apeNames{ "ape"sv };
            constexpr std::array asfNames{ "asf"sv, "wma"sv };
            constexpr std::array dsfNames{ "dsf"sv };
            constexpr std::array mpcNames{ "mpc"sv, "mpp"sv, "mp"sv };
            constexpr std::array mpegNames{ "mp3"sv, "mp2"sv, "mpeg"sv };
            constexpr std::array oggNames{ "ogg"sv, "oga"sv };
            constexpr std::array flacNames{ "flac"sv };
            constexpr std::array mp4Names{ "aac"sv, "adts"sv, "m4a"sv, "mp4"sv, "m4b"sv, "m4p"sv };
            constexpr std::array shortenNames{ "shn"sv };
            constexpr std::array trueAudioNames{ "tta"sv };
            constexpr std::array wavNames{ "wav"sv };
            constexpr std::array wavPackNames{ "wv"sv };

            std::span<const std::string_view> containerNames;

            switch (container)
            {
            case core::media::ContainerType::AIFF:
                containerNames = aiffNames;
                break;
            case core::media::ContainerType::APE:
                containerNames = apeNames;
                break;
            case core::media::ContainerType::ASF:
                containerNames = asfNames;
                break;
            case core::media::ContainerType::DSF:
                containerNames = dsfNames;
                break;
            case core::media::ContainerType::MPC:
                containerNames = mpcNames;
                break;
            case core::media::ContainerType::MPEG:
                containerNames = mpegNames;
                break;
            case core::media::ContainerType::Ogg:
                containerNames = oggNames;
                break;
            case core::media::ContainerType::FLAC:
                containerNames = flacNames;
                break;
            case core::media::ContainerType::MP4:
                containerNames = mp4Names;
                break;
            case core::media::ContainerType::Shorten:
                containerNames = shortenNames;
                break;
            case core::media::ContainerType::TrueAudio:
                containerNames = trueAudioNames;
                break;
            case core::media::ContainerType::WAV:
                containerNames = wavNames;
                break;
            case core::media::ContainerType::WavPack:
                containerNames = wavPackNames;
                break;
            }

            return std::any_of(std::cbegin(containerNames), std::cend(containerNames), [&](std::string_view containerName) { return core::stringUtils::stringCaseInsensitiveEqual(containerName, containerStr); });
        }

        bool isMatchingCodecName(core::media::CodecType codec, std::string_view codecStr)
        {
            using namespace std::literals; // for "..."sv

            constexpr std::array aacCodecNames{ "aac"sv, "adts"sv };
            constexpr std::array ac3CodecNames{ "ac3"sv, "ac-3"sv };
            constexpr std::array alacCodecNames{ "alac"sv };
            constexpr std::array apeCodecNames{ "ape"sv };
            constexpr std::array dsdCodecNames{ "dsd"sv };
            constexpr std::array flacCodecNames{ "flac"sv };
            constexpr std::array eac3CodecNames{ "eac3"sv, "e-ac3"sv, "e-ac-3"sv, "eac-3"sv };
            constexpr std::array mp3CodecNames{ "mp3"sv };
            constexpr std::array mp4alsCodecNames{ "mp4als"sv, "als"sv };
            constexpr std::array mpc7CodecNames{ "mpc7"sv, "musepack7"sv };
            constexpr std::array mpc8CodecNames{ "mpc8"sv, "musepack8"sv };
            constexpr std::array opusCodecNames{ "opus"sv };
            constexpr std::array pcmCodecNames{ "pcm"sv };
            constexpr std::array shortenCodecNames{ "shn"sv, "shorten"sv };
            constexpr std::array trueAudioCodecNames{ "tta"sv };
            constexpr std::array vorbisCodecNames{ "vorbis"sv };
            constexpr std::array wavPackCodecNames{ "wv"sv };
            constexpr std::array wma1CodecNames{ "wma1"sv, "wmav1"sv };
            constexpr std::array wma2CodecNames{ "wma2"sv, "wmav2"sv };
            constexpr std::array wma9LosslessCodecNames{ "wmalossless"sv, "wma9lossless"sv };
            constexpr std::array wma9ProCodecNames{ "wmapro"sv, "wma9pro"sv };

            std::span<const std::string_view> codecNames;
            switch (codec)
            {
            case core::media::CodecType::AAC:
                codecNames = aacCodecNames;
                break;
            case core::media::CodecType::AC3:
                codecNames = ac3CodecNames;
                break;
            case core::media::CodecType::ALAC:
                codecNames = alacCodecNames;
                break;
            case core::media::CodecType::APE:
                codecNames = apeCodecNames;
                break;
            case core::media::CodecType::DSD:
                codecNames = dsdCodecNames;
                break;
            case core::media::CodecType::EAC3:
                codecNames = eac3CodecNames;
                break;
            case core::media::CodecType::FLAC:
                codecNames = flacCodecNames;
                break;
            case core::media::CodecType::MP3:
                codecNames = mp3CodecNames;
                break;
            case core::media::CodecType::MP4ALS:
                codecNames = mp4alsCodecNames;
                break;
            case core::media::CodecType::MPC7:
                codecNames = mpc7CodecNames;
                break;
            case core::media::CodecType::MPC8:
                codecNames = mpc8CodecNames;
                break;
            case core::media::CodecType::Opus:
                codecNames = opusCodecNames;
                break;
            case core::media::CodecType::PCM:
                codecNames = pcmCodecNames;
                break;
            case core::media::CodecType::Shorten:
                codecNames = shortenCodecNames;
                break;
            case core::media::CodecType::TrueAudio:
                codecNames = trueAudioCodecNames;
                break;
            case core::media::CodecType::Vorbis:
                codecNames = vorbisCodecNames;
                break;
            case core::media::CodecType::WavPack:
                codecNames = wavPackCodecNames;
                break;
            case core::media::CodecType::WMA1:
                codecNames = wma1CodecNames;
                break;
            case core::media::CodecType::WMA2:
                codecNames = wma2CodecNames;
                break;
            case core::media::CodecType::WMA9Lossless:
                codecNames = wma9LosslessCodecNames;
                break;
            case core::media::CodecType::WMA9Pro:
                codecNames = wma9ProCodecNames;
                break;
            }

            return std::any_of(std::cbegin(codecNames), std::cend(codecNames), [&](std::string_view codecName) { return core::stringUtils::stringCaseInsensitiveEqual(codecName, codecStr); });
        }

        struct AdjustResult
        {
            enum class Type
            {
                None,
                Adjusted,
                CannotAdjust,
            };
            Type type{ Type::None };
            std::optional<unsigned> newValue;
        };

        AdjustResult adjustUsingEqualsLimitation(std::span<const std::string> values, unsigned originalValue)
        {
            if (values.size() == 1)
            {
                const auto value{ core::stringUtils::readAs<unsigned>(values.front()) };
                assert(value);
                if (originalValue == *value)
                    return AdjustResult{ .type = AdjustResult::Type::None, .newValue = std::nullopt };
            }
            else
            {
                // Get the closest allowed value *below* originalValue (we don't want to upscale)
                // Not sure if this worth doing this?

                std::optional<unsigned> closestValue;
                for (std::string_view valueStr : values)
                {
                    const auto value{ core::stringUtils::readAs<unsigned>(valueStr) };
                    assert(value);
                    if (*value == originalValue)
                        return AdjustResult{ .type = AdjustResult::Type::None, .newValue = std::nullopt };
                    if (*value < originalValue && (!closestValue || *value > *closestValue))
                        closestValue = *value;
                }
                if (closestValue)
                    return AdjustResult{ .type = AdjustResult::Type::Adjusted, .newValue = *closestValue };
            }

            // Don't really know what to do here
            return AdjustResult{ .type = AdjustResult::Type::CannotAdjust, .newValue = std::nullopt };
        }

        AdjustResult adjustUsingNotEqualsLimitation(std::span<const std::string> values, unsigned originalValue)
        {
            if (std::none_of(std::cbegin(values), std::cend(values), [&](std::string_view valueStr) {
                    const auto value{ core::stringUtils::readAs<unsigned>(valueStr) };
                    assert(value);
                    return *value == originalValue;
                }))
            {
                return AdjustResult{ .type = AdjustResult::Type::None, .newValue = std::nullopt };
            }

            // don't really know what to do here
            return AdjustResult{ .type = AdjustResult::Type::CannotAdjust, .newValue = std::nullopt };
        }

        AdjustResult adjustUsingLessThanEqualLimitation(std::span<const std::string> values, unsigned originalValue)
        {
            // Take only the first value into account
            const auto value{ core::stringUtils::readAs<unsigned>(values.front()) };
            assert(value);
            if (originalValue <= *value)
                return AdjustResult{ .type = AdjustResult::Type::None, .newValue = std::nullopt };

            return AdjustResult{ .type = AdjustResult::Type::Adjusted, .newValue = *value };
        }

        AdjustResult adjustUsingGreaterThanEqualLimitation(std::span<const std::string> values, unsigned originalValue)
        {
            // Take only the first value into account
            const auto value{ core::stringUtils::readAs<unsigned>(values.front()) };
            assert(value);

            if (originalValue >= *value)
                return AdjustResult{ .type = AdjustResult::Type::None, .newValue = std::nullopt };

            // We don't want to use a higher value than the original one (we don't want to upscale)
            return AdjustResult{ .type = AdjustResult::Type::CannotAdjust, .newValue = *value };
        }

        AdjustResult adjustUsingLimitation(Limitation::ComparisonOperator comparisonOp, std::span<const std::string> values, unsigned originalValue)
        {
            assert(values.size() >= 1);

            switch (comparisonOp)
            {
            case Limitation::ComparisonOperator::Equals:
                return adjustUsingEqualsLimitation(values, originalValue);

            case Limitation::ComparisonOperator::NotEquals:
                return adjustUsingNotEqualsLimitation(values, originalValue);

            case Limitation::ComparisonOperator::LessThanEqual:
                return adjustUsingLessThanEqualLimitation(values, originalValue);

            case Limitation::ComparisonOperator::GreaterThanEqual:
                return adjustUsingGreaterThanEqualLimitation(values, originalValue);
            }

            throw InternalErrorGenericError{ "Unhandled limitation comparison operator" };
        }

        bool isStreamCompatibleWithLimitation(const audio::AudioProperties& source, const Limitation& limitation)
        {
            if (!limitation.required)
                return true;

            // TODO handle strings
            std::optional<unsigned> valueToCheck{};
            switch (limitation.name)
            {
            case Limitation::Type::AudioBitrate:
                valueToCheck = static_cast<unsigned>(source.bitrate);
                break;
            case Limitation::Type::AudioChannels:
                valueToCheck = static_cast<unsigned>(source.channelCount);
                break;
                break;
            case Limitation::Type::AudioSamplerate:
                valueToCheck = static_cast<unsigned>(source.sampleRate);
                break;
            case Limitation::Type::AudioProfile:
                // TODO;
                break;
            case Limitation::Type::AudioBitdepth:
                if (source.bitsPerSample)
                    valueToCheck = static_cast<unsigned>(*source.bitsPerSample);
                break;
            }

            if (!valueToCheck)
                return false;

            const AdjustResult adjustResult{ adjustUsingLimitation(limitation.comparison, limitation.values, *valueToCheck) };
            return adjustResult.type == AdjustResult::Type::None;
        }

        const CodecProfile* getAudioCodecProfile(std::span<const CodecProfile> codecProfiles, core::media::CodecType codec)
        {
            for (const CodecProfile& profile : codecProfiles)
            {
                if (profile.type != "AudioCodec")
                    continue;

                if (isMatchingCodecName(codec, profile.name))
                    return &profile;
            }
            return nullptr;
        }

        std::optional<TranscodeReason> needsTranscode(const DirectPlayProfile& profile, std::span<const CodecProfile> codecProfiles, const audio::AudioProperties& source)
        {
            if (!profile.containers.empty() && std::none_of(std::cbegin(profile.containers), std::cend(profile.containers), [&](const std::string& container) { return isMatchingContainerName(source.container, container); }))
                return TranscodeReason::ContainerNotSupported;

            if (!profile.audioCodecs.empty() && std::none_of(std::cbegin(profile.audioCodecs), std::cend(profile.audioCodecs), [&](const std::string& audioCodec) { return isMatchingCodecName(source.codec, audioCodec); }))
                return TranscodeReason::AudioCodecNotSupported;

            if (!profile.protocols.empty() && std::find(std::cbegin(profile.protocols), std::cend(profile.protocols), "http") == std::cend(profile.protocols))
                return TranscodeReason::ProtocolNotSupported;

            if (profile.maxAudioChannels && source.channelCount > *profile.maxAudioChannels)
                return TranscodeReason::AudioChannelsNotSupported;

            // check potential codec profiles limitations
            if (const CodecProfile * codecProfile{ getAudioCodecProfile(codecProfiles, source.codec) })
            {
                for (const Limitation& limitation : codecProfile->limitations)
                {
                    if (!isStreamCompatibleWithLimitation(source, limitation))
                    {
                        switch (limitation.name)
                        {
                        case Limitation::Type::AudioBitrate:
                            return TranscodeReason::AudioBitrateNotSupported;
                        case Limitation::Type::AudioChannels:
                            return TranscodeReason::AudioChannelsNotSupported;
                        case Limitation::Type::AudioSamplerate:
                            return TranscodeReason::AudioSampleRateNotSupported;
                        case Limitation::Type::AudioProfile:
                            return TranscodeReason::AudioCodecNotSupported;
                        case Limitation::Type::AudioBitdepth:
                            return TranscodeReason::AudioBitdepthNotSupported;
                        }
                    }
                }
            }

            return std::nullopt;
        }

        AdjustResult applyLimitation(const audio::AudioProperties& source, const Limitation& limitation, StreamDetails& transcodedStream)
        {
            switch (limitation.name)
            {
            case Limitation::Type::AudioChannels:
                {
                    // transcodedStream.audioChannels may already be set by the transcoding profile maxAudioChannels
                    const AdjustResult adjustResult{ adjustUsingLimitation(limitation.comparison, limitation.values, transcodedStream.audioChannels ? *transcodedStream.audioChannels : source.channelCount) };
                    if (adjustResult.type == AdjustResult::Type::Adjusted)
                        transcodedStream.audioChannels = *adjustResult.newValue;
                    return adjustResult;
                }

            case Limitation::Type::AudioBitrate:
                {
                    const AdjustResult adjustResult{ adjustUsingLimitation(limitation.comparison, limitation.values, transcodedStream.audioBitrate ? *transcodedStream.audioBitrate : source.bitrate) };
                    if (adjustResult.type == AdjustResult::Type::Adjusted)
                        transcodedStream.audioBitrate = *adjustResult.newValue;
                    return adjustResult;
                }

            case Limitation::Type::AudioProfile:
                {
                    // TODO
                    AdjustResult res{ .type = AdjustResult::Type::None, .newValue = std::nullopt };
                    return res;
                }

            case Limitation::Type::AudioSamplerate:
                {
                    const AdjustResult adjustResult{ adjustUsingLimitation(limitation.comparison, limitation.values, source.sampleRate) };
                    if (adjustResult.type == AdjustResult::Type::Adjusted)
                        transcodedStream.audioSamplerate = *adjustResult.newValue;
                    return adjustResult;
                }

            case Limitation::Type::AudioBitdepth:
                {
                    if (source.bitsPerSample)
                    {
                        const AdjustResult adjustResult{ adjustUsingLimitation(limitation.comparison, limitation.values, *source.bitsPerSample) };
                        if (adjustResult.type == AdjustResult::Type::Adjusted)
                            transcodedStream.audioBitdepth = *adjustResult.newValue;

                        return adjustResult;
                    }
                }
            }

            return AdjustResult{ .type = AdjustResult::Type::CannotAdjust, .newValue = std::nullopt };
        }

        std::optional<StreamDetails> computeTranscodedStream(std::optional<std::size_t> maxAudioBitrate, const TranscodingProfile& profile, std::span<const CodecProfile> codecProfiles, const audio::AudioProperties& source)
        {
            if (profile.protocol != "http")
                return std::nullopt;

            const audio::TranscodeOutputFormat* transcodeFormat{ selectTranscodeOutputFormat(profile.container, profile.audioCodec) };
            if (!transcodeFormat)
                return std::nullopt;

            StreamDetails transcodedStream;
            transcodedStream.protocol = "http";
            transcodedStream.container = profile.container; // put back what was requested instead of our internal names
            transcodedStream.codec = profile.audioCodec;    // put back what was requested instead of our internal names

            if (core::media::isCodecLossless(source.codec))
            {
                if (!core::media::isCodecLossless(transcodeFormat->codec))
                {
                    // If coming from lossless source, maximize the bitrate if going to a non lossless source
                    // otherwise, pick a good enough value as we don't want to keep the original bitrate which does not make sense for lossy codecs
                    if (maxAudioBitrate)
                        transcodedStream.audioBitrate = maxAudioBitrate;
                    else
                        transcodedStream.audioBitrate = 256'000; // TODO, only if no bitrate limitation found? take channel count into account?
                }
                else
                {
                    // If going to a lossless codec, make sure we can respect the original bitrate
                    // technically, we could have a chance to respect the bitrate if we apply limitations, but that's not easy to have a strong garantee
                    if (maxAudioBitrate && source.bitrate > *maxAudioBitrate)
                        return std::nullopt;
                }
            }
            else
            {
                // source is lossy

                if (core::media::isCodecLossless(transcodeFormat->codec))
                    return std::nullopt; // not compatible with lossless codecs

                // let's pick the same bitrate as the lossy source
                transcodedStream.audioBitrate = source.bitrate;
            }

            if (maxAudioBitrate && source.bitrate > *maxAudioBitrate)
                transcodedStream.audioBitrate = *maxAudioBitrate;

            if (profile.maxAudioChannels && source.channelCount > *profile.maxAudioChannels)
                transcodedStream.audioChannels = *profile.maxAudioChannels;

            if (const CodecProfile * codecProfile{ getAudioCodecProfile(codecProfiles, transcodeFormat->codec) })
            {
                for (const Limitation& limitation : codecProfile->limitations)
                {
                    const AdjustResult result{ applyLimitation(source, limitation, transcodedStream) };
                    if (limitation.name == Limitation::Type::AudioBitrate && core::media::isCodecLossless(transcodeFormat->codec) && result.type == AdjustResult::Type::Adjusted)
                        return std::nullopt; // not compatible with lossless codecs

                    if (result.type == AdjustResult::Type::CannotAdjust)
                        return std::nullopt;
                }
            }

            return transcodedStream;
        }

        bool canDirectPlay(const ClientInfo& clientInfo, const audio::AudioProperties& source, std::vector<TranscodeReason>& transcodeReasons)
        {
            // Check global constraints
            if (clientInfo.maxAudioBitrate && *clientInfo.maxAudioBitrate < source.bitrate)
            {
                transcodeReasons.push_back(TranscodeReason::AudioBitrateNotSupported);
                return false;
            }

            // Check direct play profiles
            for (const DirectPlayProfile& profile : clientInfo.directPlayProfiles)
            {
                const std::optional<TranscodeReason> transcodeReason{ needsTranscode(profile, clientInfo.codecProfiles, source) };
                if (!transcodeReason)
                    return true;

                transcodeReasons.push_back(*transcodeReason);
            }

            return false;
        }
    } // namespace

    core::LiteralString transcodeReasonToString(TranscodeReason reason)
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

    const audio::TranscodeOutputFormat* selectTranscodeOutputFormat(std::string_view containerName, std::string_view codecName)
    {
        // Find a supported output format
        const auto it{ std::find_if(std::cbegin(supportedTranscodeOutputFormats), std::cend(supportedTranscodeOutputFormats), [&](const audio::TranscodeOutputFormat& format) {
            return isMatchingCodecName(format.codec, codecName) && isMatchingContainerName(format.container, containerName);
        }) };
        if (it == std::cend(supportedTranscodeOutputFormats))
            return nullptr;

        return &(*it);
    }

    TranscodeDecisionResult computeTranscodeDecision(const ClientInfo& clientInfo, const audio::AudioProperties& source)
    {
        std::vector<TranscodeReason> transcodeReasons;

        if (canDirectPlay(clientInfo, source, transcodeReasons))
            return DirectPlayResult{};

        LMS_LOG(API_SUBSONIC, DEBUG, "Direct play not possible: no compatible direct play profile found");

        // Check transcoding profiles, we have to select the first one we can handle, order is important
        for (const TranscodingProfile& profile : clientInfo.transcodingProfiles)
        {
            std::optional<StreamDetails> targetStream{ computeTranscodedStream(clientInfo.maxTranscodingAudioBitrate, profile, clientInfo.codecProfiles, source) };
            if (targetStream)
                return TranscodeResult{ .reasons = transcodeReasons, .targetStreamInfo = *targetStream };
        }

        return FailureResult{ "No compatible direct play or transcoding profile found" };
    }
} // namespace lms::api::subsonic::details
