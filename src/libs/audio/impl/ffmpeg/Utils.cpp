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

#include <array>
#include <atomic>
#include <ranges>
#include <unordered_set>
#include <vector>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/log.h>
}

#include "core/ILogger.hpp"
#include "core/LiteralString.hpp"

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

        class AvCapabilities
        {
        public:
            AvCapabilities()
            {
                ::av_log_set_callback(avLogCallback);

                void* opaque{};
                while (const AVInputFormat * fmt{ ::av_demuxer_iterate(&opaque) })
                {
                    if (const auto c{ containerFromFormatName(fmt->name) })
                        _supportedDemuxers.insert(*c);
                }

                opaque = nullptr;
                while (const AVOutputFormat * fmt{ ::av_muxer_iterate(&opaque) })
                {
                    if (const auto c{ containerFromFormatName(fmt->name) })
                        _supportedMuxers.insert(*c);
                }

                opaque = nullptr;
                while (const AVCodec * avCodec{ ::av_codec_iterate(&opaque) })
                {
                    if (avCodec->type != AVMEDIA_TYPE_AUDIO)
                        continue;

                    const auto c{ codecFromAVCodecId(avCodec->id) };
                    if (!c)
                        continue;

                    if (::av_codec_is_decoder(avCodec))
                        _supportedDecoders.insert(*c);
                    if (::av_codec_is_encoder(avCodec))
                        _supportedEncoders.insert(*c);
                }

                _supportedDemuxerExtensions = buildSupportedDemuxerExtensions();

                auto containerName{ [](core::media::Container c) { return std::string{ core::media::containerToString(c).str() }; } };
                auto codecName{ [](core::media::Codec c) { return std::string{ core::media::getCodecDesc(c).name.str() }; } };
                auto extensionName{ [](const std::filesystem::path& p) { return p.string(); } };

                LMS_LOG(AUDIO, INFO, "Supported demuxers: " << core::stringUtils::joinStrings(_supportedDemuxers | std::views::transform(containerName), ' '));
                LMS_LOG(AUDIO, INFO, "Supported muxers: " << core::stringUtils::joinStrings(_supportedMuxers | std::views::transform(containerName), ' '));
                LMS_LOG(AUDIO, INFO, "Supported decoders: " << core::stringUtils::joinStrings(_supportedDecoders | std::views::transform(codecName), ' '));
                LMS_LOG(AUDIO, INFO, "Supported encoders: " << core::stringUtils::joinStrings(_supportedEncoders | std::views::transform(codecName), ' '));
                LMS_LOG(AUDIO, INFO, "Supported demuxer extensions: " << core::stringUtils::joinStrings(_supportedDemuxerExtensions | std::views::transform(extensionName), ' '));
            }

            bool isDemuxingSupported(core::media::Container container) const { return _supportedDemuxers.contains(container); }
            bool isDecodingSupported(core::media::Codec codec) const { return _supportedDecoders.contains(codec); }
            bool isMuxingSupported(core::media::Container container) const { return _supportedMuxers.contains(container); }
            bool isEncodingSupported(core::media::Codec codec) const { return _supportedEncoders.contains(codec); }

            std::span<const std::filesystem::path> getSupportedDemuxerExtensions() const { return _supportedDemuxerExtensions; }

        private:
            [[nodiscard]] std::vector<std::filesystem::path> buildSupportedDemuxerExtensions() const
            {
                struct ExtensionContainer
                {
                    std::string_view extension;
                    core::media::Container container;
                };

                constexpr std::array candidates{
                    ExtensionContainer{ ".aac", core::media::Container::MPEG }, // raw ADTS AAC, bundled under MPEG like taglib does
                    ExtensionContainer{ ".aif", core::media::Container::AIFF },
                    ExtensionContainer{ ".aifc", core::media::Container::AIFF },
                    ExtensionContainer{ ".aiff", core::media::Container::AIFF },
                    ExtensionContainer{ ".alac", core::media::Container::MP4 }, // ALAC is muxed in MP4, same as .m4a
                    ExtensionContainer{ ".ape", core::media::Container::APE },
                    ExtensionContainer{ ".dsf", core::media::Container::DSF },
                    ExtensionContainer{ ".flac", core::media::Container::FLAC },
                    ExtensionContainer{ ".m4a", core::media::Container::MP4 },
                    ExtensionContainer{ ".m4b", core::media::Container::MP4 },
                    ExtensionContainer{ ".mp3", core::media::Container::MPEG },
                    ExtensionContainer{ ".mpc", core::media::Container::MPC },
                    ExtensionContainer{ ".oga", core::media::Container::Ogg },
                    ExtensionContainer{ ".ogg", core::media::Container::Ogg },
                    ExtensionContainer{ ".opus", core::media::Container::Ogg },
                    ExtensionContainer{ ".shn", core::media::Container::Shorten },
                    ExtensionContainer{ ".tta", core::media::Container::TrueAudio },
                    ExtensionContainer{ ".wav", core::media::Container::WAV },
                    ExtensionContainer{ ".wma", core::media::Container::ASF },
                    ExtensionContainer{ ".wv", core::media::Container::WavPack },
                };

                std::vector<std::filesystem::path> result;
                for (const auto& [extension, container] : candidates)
                {
                    if (isDemuxingSupported(container))
                        result.emplace_back(extension);
                }
                return result;
            }

            std::unordered_set<core::media::Container> _supportedDemuxers;
            std::unordered_set<core::media::Container> _supportedMuxers;
            std::unordered_set<core::media::Codec> _supportedDecoders;
            std::unordered_set<core::media::Codec> _supportedEncoders;
            std::vector<std::filesystem::path> _supportedDemuxerExtensions;
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

    std::optional<core::media::Container> containerFromFormatName(std::string_view name)
    {
        if (name == "aiff" || name == "aifc" || name == "aif")
            return core::media::Container::AIFF;
        if (name == "ape")
            return core::media::Container::APE;
        if (name.starts_with("asf"))
            return core::media::Container::ASF;
        if (name == "dsf")
            return core::media::Container::DSF;
        if (name == "flac")
            return core::media::Container::FLAC;
        if (name.find("mp4") != std::string_view::npos || name == "mov") // "mov" is a separate muxer name, same underlying format family
            return core::media::Container::MP4;
        if (name.starts_with("mpc"))
            return core::media::Container::MPC;
        if (name == "mp3" || name == "aac") // raw ADTS AAC has no container of its own; taglib bundles it under MPEG too
            return core::media::Container::MPEG;
        if (name == "ogg" || name == "opus") // "opus" is a separate, Ogg-Opus-specific muxer name
            return core::media::Container::Ogg;
        if (name == "shn")
            return core::media::Container::Shorten;
        if (name == "tta")
            return core::media::Container::TrueAudio;
        if (name == "wav")
            return core::media::Container::WAV;
        if (name == "wv")
            return core::media::Container::WavPack;

        return std::nullopt;
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

    bool isDemuxingSupported(core::media::Container container)
    {
        return getCapabilities().isDemuxingSupported(container);
    }

    bool isDecodingSupported(core::media::Codec codec)
    {
        return getCapabilities().isDecodingSupported(codec);
    }

    bool isMuxingSupported(core::media::Container container)
    {
        return getCapabilities().isMuxingSupported(container);
    }

    bool isEncodingSupported(core::media::Codec codec)
    {
        return getCapabilities().isEncodingSupported(codec);
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