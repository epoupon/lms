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

#include "Utils.hpp"
#include "core/String.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avstring.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/log.h>
}

#include "core/ILogger.hpp"
#include "core/LiteralString.hpp"
#include "core/media/AudioFormat.hpp"

#include "audio/Exception.hpp"

#include "Exception.hpp"

namespace lms::audio::ffmpeg::utils
{
    namespace
    {
        std::atomic<bool> isInitialized{ false };

        core::LiteralString avLogLevelToStr(int level)
        {
            switch (level)
            {
            case AV_LOG_TRACE:
                return "trace";
            case AV_LOG_DEBUG:
                return "debug";
            case AV_LOG_VERBOSE:
                return "verbose";
            case AV_LOG_INFO:
                return "info";
            case AV_LOG_WARNING:
                return "warning";
            case AV_LOG_ERROR:
                return "error";
            case AV_LOG_FATAL:
                return "fatal";
            case AV_LOG_PANIC:
                return "panic";
            default:
                return "unknown";
            }
        }

        void avLogCallback(void*, int level, const char* fmt, va_list vl)
        {
            if (!core::Service<core::logging::ILogger>::get() || core::Service<core::logging::ILogger>::get()->isSeverityActive(core::logging::Severity::DEBUG))
                return;

            if (level > AV_LOG_WARNING)
                return;

            std::array<char, 256> buffer{ 0 };
            if (std::vsnprintf(buffer.data(), buffer.size(), fmt, vl) > 0)
            {
                std::string_view str{ buffer.data() };
                str = core::stringUtils::stringTrimEnd(str, " \t\r\n");

                // TODO translate levels?
                LMS_LOG(AUDIO, DEBUG, "[FFmpeg] [" << avLogLevelToStr(level) << "] " << str);
            }
        }

        constexpr std::size_t maxAlternateNames{ 2 };
        using CandidateNames = std::array<core::LiteralString, maxAlternateNames>;

        // One entry per Codec that we want to support, preferred first
        struct EncoderNames
        {
            core::media::Codec codec;
            CandidateNames names;
        };

        constexpr std::array encoderNamesByCodec{
            EncoderNames{ .codec = core::media::Codec::AAC, .names = { "aac" } },
            EncoderNames{ .codec = core::media::Codec::AC3, .names = { "ac3", "ac3_fixed" } },
            EncoderNames{ .codec = core::media::Codec::ALAC, .names = { "alac" } },
            EncoderNames{ .codec = core::media::Codec::EAC3, .names = { "eac3" } },
            EncoderNames{ .codec = core::media::Codec::FLAC, .names = { "flac" } },
            EncoderNames{ .codec = core::media::Codec::MP3, .names = { "libmp3lame", "libshine" } },
            EncoderNames{ .codec = core::media::Codec::Opus, .names = { "libopus", "opus" } },
            EncoderNames{ .codec = core::media::Codec::TrueAudio, .names = { "tta" } },
            EncoderNames{ .codec = core::media::Codec::Vorbis, .names = { "libvorbis", "vorbis" } },
            EncoderNames{ .codec = core::media::Codec::WavPack, .names = { "wavpack" } },
            EncoderNames{ .codec = core::media::Codec::WMA1, .names = { "wmav1" } },
            EncoderNames{ .codec = core::media::Codec::WMA2, .names = { "wmav2" } },
        };

        struct AudioFormatCapability
        {
            core::media::AudioFormat format;
            CandidateNames demuxerNames;
            CandidateNames muxerNames; // empty means we don't support it (like requires a seekable output)
        };

        constexpr std::array audioFormatCapabilities{
            AudioFormatCapability{ .format = { core::media::Container::AIFF, core::media::Codec::PCM }, .demuxerNames = { "aiff" }, .muxerNames = {} },
            AudioFormatCapability{ .format = { core::media::Container::APE, core::media::Codec::APE }, .demuxerNames = { "ape" }, .muxerNames = { "ape" } },
            AudioFormatCapability{ .format = { core::media::Container::ASF, core::media::Codec::WMA1 }, .demuxerNames = { "asf" }, .muxerNames = { "asf", "asf_stream" } },
            AudioFormatCapability{ .format = { core::media::Container::ASF, core::media::Codec::WMA2 }, .demuxerNames = { "asf" }, .muxerNames = { "asf", "asf_stream" } },
            AudioFormatCapability{ .format = { core::media::Container::ASF, core::media::Codec::WMA9Pro }, .demuxerNames = { "asf" }, .muxerNames = { "asf", "asf_stream" } },
            AudioFormatCapability{ .format = { core::media::Container::ASF, core::media::Codec::WMA9Lossless }, .demuxerNames = { "asf" }, .muxerNames = { "asf", "asf_stream" } },
            AudioFormatCapability{ .format = { core::media::Container::DSF, core::media::Codec::DSD }, .demuxerNames = { "dsf" }, .muxerNames = { "dsf" } },
            AudioFormatCapability{ .format = { core::media::Container::FLAC, core::media::Codec::FLAC }, .demuxerNames = { "flac" }, .muxerNames = { "flac" } },
            AudioFormatCapability{ .format = { core::media::Container::MP4, core::media::Codec::AAC }, .demuxerNames = { "mp4" }, .muxerNames = {} },
            AudioFormatCapability{ .format = { core::media::Container::MP4, core::media::Codec::ALAC }, .demuxerNames = { "mp4" }, .muxerNames = {} },
            AudioFormatCapability{ .format = { core::media::Container::MPC, core::media::Codec::MPC7 }, .demuxerNames = { "mpc" }, .muxerNames = { "mpc" } },
            AudioFormatCapability{ .format = { core::media::Container::MPC, core::media::Codec::MPC8 }, .demuxerNames = { "mpc8" }, .muxerNames = { "mpc8" } },
            AudioFormatCapability{ .format = { core::media::Container::MPEG, core::media::Codec::MP3 }, .demuxerNames = { "mp3" }, .muxerNames = { "mp3" } },
            AudioFormatCapability{ .format = { core::media::Container::MPEG, core::media::Codec::AAC }, .demuxerNames = { "aac" }, .muxerNames = { "adts" } },
            AudioFormatCapability{ .format = { core::media::Container::Ogg, core::media::Codec::FLAC }, .demuxerNames = { "ogg" }, .muxerNames = { "ogg", "oga" } },
            AudioFormatCapability{ .format = { core::media::Container::Ogg, core::media::Codec::Vorbis }, .demuxerNames = { "ogg" }, .muxerNames = { "ogg" } },
            AudioFormatCapability{ .format = { core::media::Container::Ogg, core::media::Codec::Opus }, .demuxerNames = { "ogg" }, .muxerNames = { "ogg", "opus" } },
            AudioFormatCapability{ .format = { core::media::Container::Shorten, core::media::Codec::Shorten }, .demuxerNames = { "shn" }, .muxerNames = { "shn" } },
            AudioFormatCapability{ .format = { core::media::Container::TrueAudio, core::media::Codec::TrueAudio }, .demuxerNames = { "tta" }, .muxerNames = { "tta" } },
            AudioFormatCapability{ .format = { core::media::Container::WAV, core::media::Codec::PCM }, .demuxerNames = { "wav" }, .muxerNames = { "wav" } },
            AudioFormatCapability{ .format = { core::media::Container::WavPack, core::media::Codec::WavPack }, .demuxerNames = { "wv" }, .muxerNames = { "wv" } },
        };

        const AVInputFormat* findFirstAvailableDemuxer(const CandidateNames& names)
        {
            for (const core::LiteralString& name : names)
            {
                if (!name.empty())
                {
                    if (const AVInputFormat * demuxer{ ::av_find_input_format(name.c_str()) })
                        return demuxer;
                }
            }

            return nullptr;
        }

        const AVOutputFormat* findFirstAvailableMuxer(const CandidateNames& names)
        {
            for (const core::LiteralString& name : names)
            {
                if (!name.empty())
                {
                    if (const AVOutputFormat * muxer{ ::av_guess_format(name.c_str(), nullptr, nullptr) })
                        return muxer;
                }
            }

            return nullptr;
        }

        const AVCodec* findFirstAvailableEncoder(const CandidateNames& names)
        {
            for (const core::LiteralString& name : names)
            {
                if (!name.empty())
                {
                    if (const AVCodec * encoder{ ::avcodec_find_encoder_by_name(name.c_str()) })
                        return encoder;
                }
            }

            return nullptr;
        }

        template<typename T>
        std::span<const T> getSupportedConfig(const AVCodec& encoder, ::AVCodecConfig config)
        {
            const void* values{};
            int count{};
            if (::avcodec_get_supported_config(nullptr, &encoder, config, 0, &values, &count) < 0 || !values)
                return {};

            return std::span<const T>{ static_cast<const T*>(values), static_cast<std::size_t>(count) };
        }

        struct EncoderInfo
        {
            const AVCodec* encoder;
            std::vector<::AVSampleFormat> supportedSampleFormats;        // empty means the encoder does not restrict this
            std::vector<int> supportedSampleRates;                       // empty means the encoder does not restrict this
            std::vector<const AVChannelLayout*> supportedChannelLayouts; // empty means the encoder does not restrict this
        };

        EncoderInfo buildEncoderInfo(const AVCodec& encoder)
        {
            EncoderInfo info{};
            info.encoder = &encoder;

            const std::span<const ::AVSampleFormat> formats{ getSupportedConfig<::AVSampleFormat>(encoder, AV_CODEC_CONFIG_SAMPLE_FORMAT) };
            info.supportedSampleFormats.assign(std::cbegin(formats), std::cend(formats));

            const std::span<const int> sampleRates{ getSupportedConfig<int>(encoder, AV_CODEC_CONFIG_SAMPLE_RATE) };
            info.supportedSampleRates.assign(std::cbegin(sampleRates), std::cend(sampleRates));

            for (const AVChannelLayout& layout : getSupportedConfig<AVChannelLayout>(encoder, AV_CODEC_CONFIG_CHANNEL_LAYOUT))
                info.supportedChannelLayouts.push_back(&layout);

            return info;
        }

        struct SampleFormatList
        {
            std::span<const ::AVSampleFormat> formats;
        };

        struct SampleRateList
        {
            std::span<const int> sampleRates;
        };

        struct ChannelLayoutList
        {
            std::span<const AVChannelLayout* const> layouts;
        };

        std::ostream& operator<<(std::ostream& os, SampleFormatList list)
        {
            if (list.formats.empty())
                return os << "any";

            for (std::size_t i{}; i < list.formats.size(); ++i)
            {
                if (i > 0)
                    os << ' ';
                os << (::av_get_sample_fmt_name(list.formats[i]) ? ::av_get_sample_fmt_name(list.formats[i]) : "?");
            }

            return os;
        }

        std::ostream& operator<<(std::ostream& os, SampleRateList list)
        {
            if (list.sampleRates.empty())
                return os << "any";

            for (std::size_t i{}; i < list.sampleRates.size(); ++i)
            {
                if (i > 0)
                    os << ' ';
                os << list.sampleRates[i];
            }

            return os;
        }

        std::ostream& operator<<(std::ostream& os, ChannelLayoutList list)
        {
            if (list.layouts.empty())
                return os << "any";

            std::array<char, 64> buffer{};
            for (std::size_t i{}; i < list.layouts.size(); ++i)
            {
                if (i > 0)
                    os << ' ';
                ::av_channel_layout_describe(list.layouts[i], buffer.data(), buffer.size());
                os << buffer.data();
            }

            return os;
        }

        class AvCapabilities
        {
        public:
            AvCapabilities()
            {
                ::av_log_set_callback(avLogCallback);

                std::unordered_set<core::media::Codec> supportedDecoders;

                void* opaque{};
                while (const AVCodec * avCodec{ ::av_codec_iterate(&opaque) })
                {
                    if (avCodec->type == AVMEDIA_TYPE_AUDIO && ::av_codec_is_decoder(avCodec))
                    {
                        if (const auto codec{ codecFromAVCodecId(avCodec->id) })
                            supportedDecoders.insert(*codec);
                    }
                }

                for (const EncoderNames& entry : encoderNamesByCodec)
                {
                    const AVCodec* encoder{ findFirstAvailableEncoder(entry.names) };
                    if (!encoder)
                        continue;

                    EncoderInfo info{ buildEncoderInfo(*encoder) };
                    LMS_LOG(AUDIO, INFO, "Codec " << core::media::getCodecDesc(entry.codec).name.str() << ", encoder '" << encoder->name << "': sample formats = [" << SampleFormatList{ info.supportedSampleFormats } << "], sample rates = [" << SampleRateList{ info.supportedSampleRates } << "], channel layouts = [" << ChannelLayoutList{ info.supportedChannelLayouts } << "]");

                    _encoderInfoByCodec[entry.codec] = std::move(info);
                }

                for (const AudioFormatCapability& entry : audioFormatCapabilities)
                {
                    if (const AVOutputFormat * muxer{ resolveMuxerForCodec(entry.format.codec, entry.muxerNames) })
                        _muxerByContainerCodec[entry.format.container][entry.format.codec] = muxer;

                    if (!findFirstAvailableDemuxer(entry.demuxerNames))
                        continue;

                    _supportedDemuxerFormats.push_back(entry);

                    if (supportedDecoders.contains(entry.format.codec))
                        _decodableCodecsByContainer[entry.format.container].insert(entry.format.codec);

                    for (std::string_view extension : core::media::getExtensionsForAudioFormat(entry.format))
                    {
                        if (std::find(std::cbegin(_supportedDemuxerExtensions), std::cend(_supportedDemuxerExtensions), extension) == std::cend(_supportedDemuxerExtensions))
                            _supportedDemuxerExtensions.emplace_back(extension);
                    }
                }

                auto containerName{ [](core::media::Container c) { return std::string{ core::media::containerToString(c).str() }; } };
                auto codecName{ [](core::media::Codec c) { return std::string{ core::media::getCodecDesc(c).name.str() }; } };

                std::vector<std::string> decodablePairs;
                for (const auto& [container, codecs] : _decodableCodecsByContainer)
                {
                    for (const core::media::Codec codec : codecs)
                        decodablePairs.push_back(containerName(container) + "/" + codecName(codec));
                }

                std::vector<std::string> encodablePairs;
                for (const auto& [container, codecs] : _muxerByContainerCodec)
                {
                    for (const auto& [codec, muxer] : codecs)
                        encodablePairs.push_back(containerName(container) + "/" + codecName(codec));
                }

                LMS_LOG(AUDIO, INFO, "Supported decoding: " << core::stringUtils::joinStrings(decodablePairs, ' '));
                LMS_LOG(AUDIO, INFO, "Supported encoding: " << core::stringUtils::joinStrings(encodablePairs, ' '));
                LMS_LOG(AUDIO, INFO, "Supported demuxer extensions: " << core::stringUtils::joinStrings(_supportedDemuxerExtensions | std::views::transform([](const std::filesystem::path& p) { return p.c_str(); }), ' '));
            }

            bool isDecodingSupported(core::media::Container container, core::media::Codec codec) const
            {
                const auto it{ _decodableCodecsByContainer.find(container) };
                return it != _decodableCodecsByContainer.end() && it->second.contains(codec);
            }

            std::span<const std::filesystem::path> getSupportedDemuxerExtensions() const { return _supportedDemuxerExtensions; }

            const AVCodec* getEncoderForCodec(core::media::Codec codec) const
            {
                const auto it{ _encoderInfoByCodec.find(codec) };
                return it != _encoderInfoByCodec.end() ? it->second.encoder : nullptr;
            }

            std::span<const ::AVSampleFormat> getSupportedSampleFormats(core::media::Codec codec) const
            {
                const auto it{ _encoderInfoByCodec.find(codec) };
                return it != _encoderInfoByCodec.end() ? std::span{ it->second.supportedSampleFormats } : std::span<const ::AVSampleFormat>{};
            }

            std::span<const int> getSupportedSampleRates(core::media::Codec codec) const
            {
                const auto it{ _encoderInfoByCodec.find(codec) };
                return it != _encoderInfoByCodec.end() ? std::span{ it->second.supportedSampleRates } : std::span<const int>{};
            }

            std::span<const AVChannelLayout* const> getSupportedChannelLayouts(core::media::Codec codec) const
            {
                const auto it{ _encoderInfoByCodec.find(codec) };
                return it != _encoderInfoByCodec.end() ? std::span{ it->second.supportedChannelLayouts } : std::span<const AVChannelLayout* const>{};
            }

            const AVOutputFormat* findMuxerForCodec(core::media::Container container, core::media::Codec codec) const
            {
                const auto containerIt{ _muxerByContainerCodec.find(container) };
                if (containerIt == _muxerByContainerCodec.end())
                    return nullptr;

                const auto codecIt{ containerIt->second.find(codec) };
                return codecIt != containerIt->second.end() ? codecIt->second : nullptr;
            }

            std::optional<core::media::Container> containerFromDemuxerName(const char* iformatName) const
            {
                for (const AudioFormatCapability& entry : _supportedDemuxerFormats)
                {
                    for (const core::LiteralString& candidate : entry.demuxerNames)
                    {
                        if (!candidate.empty() && ::av_match_name(candidate.c_str(), iformatName))
                            return entry.format.container;
                    }
                }

                return std::nullopt;
            }

        private:
            const AVOutputFormat* resolveMuxerForCodec(core::media::Codec codec, const CandidateNames& muxerNames) const
            {
                return _encoderInfoByCodec.contains(codec) ? findFirstAvailableMuxer(muxerNames) : nullptr;
            }

            std::vector<std::filesystem::path> _supportedDemuxerExtensions;
            std::vector<AudioFormatCapability> _supportedDemuxerFormats;
            std::unordered_map<core::media::Codec, EncoderInfo> _encoderInfoByCodec;
            std::unordered_map<core::media::Container, std::unordered_map<core::media::Codec, const AVOutputFormat*>> _muxerByContainerCodec;
            std::unordered_map<core::media::Container, std::unordered_set<core::media::Codec>> _decodableCodecsByContainer;
        };

        const AvCapabilities& getCapabilities()
        {
            static const AvCapabilities instance;
            return instance;
        }
    } // namespace

    std::string averrorToString(int error)
    {
        std::array<char, 128> buf{ 0 };

        if (::av_strerror(error, buf.data(), buf.size()) == 0)
            return buf.data();

        return "Unknown error";
    }

    std::optional<core::media::Container> containerFromDemuxerName(const char* name)
    {
        return getCapabilities().containerFromDemuxerName(name);
    }

    std::optional<core::media::Codec> codecFromAVCodecId(AVCodecID codec)
    {
        switch (codec)
        {
        case AV_CODEC_ID_AAC:
            return core::media::Codec::AAC;
        case AV_CODEC_ID_AC3:
            return core::media::Codec::AC3;
        case AV_CODEC_ID_ALAC:
            return core::media::Codec::ALAC;
        case AV_CODEC_ID_APE:
            return core::media::Codec::APE;
        case AV_CODEC_ID_DSD_LSBF:
        case AV_CODEC_ID_DSD_LSBF_PLANAR:
        case AV_CODEC_ID_DSD_MSBF:
        case AV_CODEC_ID_DSD_MSBF_PLANAR:
            return core::media::Codec::DSD;
        case AV_CODEC_ID_EAC3:
            return core::media::Codec::EAC3;
        case AV_CODEC_ID_FLAC:
            return core::media::Codec::FLAC;
        case AV_CODEC_ID_MP3:
            return core::media::Codec::MP3;
        case AV_CODEC_ID_MP4ALS:
            return core::media::Codec::MP4ALS;
        case AV_CODEC_ID_MUSEPACK7:
            return core::media::Codec::MPC7;
        case AV_CODEC_ID_MUSEPACK8:
            return core::media::Codec::MPC8;
        case AV_CODEC_ID_OPUS:
            return core::media::Codec::Opus;
        case AV_CODEC_ID_PCM_S16LE:
        case AV_CODEC_ID_PCM_S16BE:
        case AV_CODEC_ID_PCM_U16LE:
        case AV_CODEC_ID_PCM_U16BE:
        case AV_CODEC_ID_PCM_S8:
        case AV_CODEC_ID_PCM_U8:
        case AV_CODEC_ID_PCM_S32LE:
        case AV_CODEC_ID_PCM_S32BE:
        case AV_CODEC_ID_PCM_U32LE:
        case AV_CODEC_ID_PCM_U32BE:
        case AV_CODEC_ID_PCM_S24LE:
        case AV_CODEC_ID_PCM_S24BE:
        case AV_CODEC_ID_PCM_U24LE:
        case AV_CODEC_ID_PCM_U24BE:
        case AV_CODEC_ID_PCM_S16LE_PLANAR:
        case AV_CODEC_ID_PCM_F32BE:
        case AV_CODEC_ID_PCM_F32LE:
        case AV_CODEC_ID_PCM_F64BE:
        case AV_CODEC_ID_PCM_F64LE:
        case AV_CODEC_ID_PCM_S8_PLANAR:
        case AV_CODEC_ID_PCM_S24LE_PLANAR:
        case AV_CODEC_ID_PCM_S32LE_PLANAR:
        case AV_CODEC_ID_PCM_S16BE_PLANAR:
        case AV_CODEC_ID_PCM_S64LE:
        case AV_CODEC_ID_PCM_S64BE:
        case AV_CODEC_ID_PCM_F16LE:
        case AV_CODEC_ID_PCM_F24LE:
        case AV_CODEC_ID_PCM_MULAW:
        case AV_CODEC_ID_PCM_ALAW:
        case AV_CODEC_ID_ADPCM_G726:
        case AV_CODEC_ID_ADPCM_G722:
        case AV_CODEC_ID_ADPCM_G726LE:
            return core::media::Codec::PCM;
        case AV_CODEC_ID_SHORTEN:
            return core::media::Codec::Shorten;
        case AV_CODEC_ID_TTA:
            return core::media::Codec::TrueAudio;
        case AV_CODEC_ID_VORBIS:
            return core::media::Codec::Vorbis;
        case AV_CODEC_ID_WAVPACK:
            return core::media::Codec::WavPack;
        case AV_CODEC_ID_WMALOSSLESS:
            return core::media::Codec::WMA9Lossless;
        case AV_CODEC_ID_WMAPRO:
            return core::media::Codec::WMA9Pro;
        case AV_CODEC_ID_WMAV1:
            return core::media::Codec::WMA1;
        case AV_CODEC_ID_WMAV2:
            return core::media::Codec::WMA2;

        default:
            return std::nullopt;
        }
    }

    std::span<const std::filesystem::path> getSupportedDemuxerExtensions()
    {
        return getCapabilities().getSupportedDemuxerExtensions();
    }

    bool isDecodingSupported(core::media::Container container, core::media::Codec codec)
    {
        return getCapabilities().isDecodingSupported(container, codec);
    }

    bool isCodecMuxingSupported(core::media::Container container, core::media::Codec codec)
    {
        return getCapabilities().findMuxerForCodec(container, codec) != nullptr;
    }

    const AVCodec* findEncoder(core::media::Codec codec)
    {
        const AVCodec* encoder{ getCapabilities().getEncoderForCodec(codec) };
        if (!encoder)
            throw Exception{ "No encoder available for codec " + std::string{ core::media::getCodecDesc(codec).name.str() } + ": check the FFmpeg libraries this build is linked against" };

        return encoder;
    }

    const AVOutputFormat* findMuxer(core::media::Container container, core::media::Codec codec)
    {
        const AVOutputFormat* muxer{ getCapabilities().findMuxerForCodec(container, codec) };
        if (!muxer)
            throw Exception{ "Codec " + std::string{ core::media::getCodecDesc(codec).name.str() } + " cannot be muxed into container " + std::string{ core::media::containerToString(container).str() } + ": not supported by this build" };

        return muxer;
    }

    std::span<const ::AVSampleFormat> getSupportedSampleFormats(core::media::Codec codec)
    {
        return getCapabilities().getSupportedSampleFormats(codec);
    }

    std::span<const int> getSupportedSampleRates(core::media::Codec codec)
    {
        return getCapabilities().getSupportedSampleRates(codec);
    }

    std::span<const AVChannelLayout* const> getSupportedChannelLayouts(core::media::Codec codec)
    {
        return getCapabilities().getSupportedChannelLayouts(codec);
    }

    PcmSampleType toPcmSampleType(::AVSampleFormat format)
    {
        switch (format)
        {
        case AV_SAMPLE_FMT_S16:
        case AV_SAMPLE_FMT_S16P:
            return PcmSampleType::Signed16;
        case AV_SAMPLE_FMT_S32:
        case AV_SAMPLE_FMT_S32P:
            return PcmSampleType::Signed32;
        case AV_SAMPLE_FMT_FLT:
        case AV_SAMPLE_FMT_FLTP:
            return PcmSampleType::Float32;
        case AV_SAMPLE_FMT_DBL:
        case AV_SAMPLE_FMT_DBLP:
            return PcmSampleType::Float64;

        default:
            break;
        }

        throw Exception{ "Unsupported sample format " + std::string{ ::av_get_sample_fmt_name(format) ? ::av_get_sample_fmt_name(format) : "?" } };
    }

    ::AVSampleFormat toAvSampleFormat(PcmSampleType type, bool planar)
    {
        switch (type)
        {
        case PcmSampleType::Signed16:
            return planar ? AV_SAMPLE_FMT_S16P : AV_SAMPLE_FMT_S16;
        case PcmSampleType::Signed32:
            return planar ? AV_SAMPLE_FMT_S32P : AV_SAMPLE_FMT_S32;
        case PcmSampleType::Float32:
            return planar ? AV_SAMPLE_FMT_FLTP : AV_SAMPLE_FMT_FLT;
        case PcmSampleType::Float64:
            return planar ? AV_SAMPLE_FMT_DBLP : AV_SAMPLE_FMT_DBL;
        }

        throw Exception{ "Unsupported PcmSampleType" };
    }

    void init()
    {
        assert(!isInitialized);

        LMS_LOG(AUDIO, INFO, "Initializing ffmpeg backend...");
        isInitialized = true;
        getCapabilities(); // init
        LMS_LOG(AUDIO, INFO, "ffmpeg backend init done");
    }

    bool isInit()
    {
        return isInitialized;
    }
} // namespace lms::audio::ffmpeg::utils