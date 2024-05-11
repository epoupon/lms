/*
 * Copyright (C) 2015 Emeric Poupon
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

#include "AudioFile.hpp"

extern "C"
{
#define __STDC_CONSTANT_MACROS
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}

#include <array>
#include <map>
#include <unordered_map>

#include "core/ILogger.hpp"
#include "core/String.hpp"

namespace lms::av
{
    namespace
    {
        std::string averror_to_string(int error)
        {
            std::array<char, 128> buf = { 0 };

            if (::av_strerror(error, buf.data(), buf.size()) == 0)
                return &buf[0];
            else
                return "Unknown error";
        }

        class AudioFileException : public Exception
        {
        public:
            AudioFileException(int avError)
                : Exception{ "AudioFileException: " + averror_to_string(avError) }
            {}
        };

        void getMetaDataFromDictionnary(AVDictionary* dictionnary, AudioFile::MetadataMap& res)
        {
            if (!dictionnary)
                return;

            AVDictionaryEntry* tag = NULL;
            while ((tag = ::av_dict_get(dictionnary, "", tag, AV_DICT_IGNORE_SUFFIX)))
            {
                res[core::stringUtils::stringToUpper(tag->key)] = tag->value;
            }
        }

        DecodingCodec avcodecToDecodingCodec(AVCodecID codec)
        {
            switch (codec)
            {
            case AV_CODEC_ID_MP3:          return DecodingCodec::MP3;
            case AV_CODEC_ID_AAC:          return DecodingCodec::AAC;
            case AV_CODEC_ID_AC3:          return DecodingCodec::AC3;
            case AV_CODEC_ID_VORBIS:       return DecodingCodec::VORBIS;
            case AV_CODEC_ID_WMAV1:        return DecodingCodec::WMAV1;
            case AV_CODEC_ID_WMAV2:        return DecodingCodec::WMAV2;
            case AV_CODEC_ID_FLAC:         return DecodingCodec::FLAC;
            case AV_CODEC_ID_ALAC:         return DecodingCodec::ALAC;
            case AV_CODEC_ID_WAVPACK:      return DecodingCodec::WAVPACK;
            case AV_CODEC_ID_MUSEPACK7:    return DecodingCodec::MUSEPACK7;
            case AV_CODEC_ID_MUSEPACK8:    return DecodingCodec::MUSEPACK8;
            case AV_CODEC_ID_APE:          return DecodingCodec::APE;
            case AV_CODEC_ID_EAC3:         return DecodingCodec::EAC3;
            case AV_CODEC_ID_MP4ALS:       return DecodingCodec::MP4ALS;
            case AV_CODEC_ID_OPUS:         return DecodingCodec::OPUS;
            case AV_CODEC_ID_SHORTEN:      return DecodingCodec::SHORTEN;
            default:
                return DecodingCodec::UNKNOWN;
            }
        }
    }

    std::unique_ptr<IAudioFile> parseAudioFile(const std::filesystem::path& p)
    {
        return std::make_unique<AudioFile>(p);
    }

    AudioFile::AudioFile(const std::filesystem::path& p)
        : _p{ p }
    {
        int error{ avformat_open_input(&_context, _p.string().c_str(), nullptr, nullptr) };
        if (error < 0)
        {
            LMS_LOG(AV, ERROR, "Cannot open " << _p.string() << ": " << averror_to_string(error));
            throw AudioFileException{ error };
        }

        error = avformat_find_stream_info(_context, nullptr);
        if (error < 0)
        {
            LMS_LOG(AV, ERROR, "Cannot find stream information on " << _p.string() << ": " << averror_to_string(error));
            avformat_close_input(&_context);
            throw AudioFileException{ error };
        }
    }

    AudioFile::~AudioFile()
    {
        avformat_close_input(&_context);
    }

    const std::filesystem::path& AudioFile::getPath() const
    {
        return _p;
    }

    ContainerInfo AudioFile::getContainerInfo() const
    {
        ContainerInfo info;
        info.bitrate = _context->bit_rate;
        info.duration = std::chrono::milliseconds{ _context->duration == AV_NOPTS_VALUE ? 0 : _context->duration / AV_TIME_BASE * 1000 };
        info.name = _context->iformat->name;

        return info;
    }

    AudioFile::MetadataMap AudioFile::getMetaData() const
    {
        MetadataMap res;

        getMetaDataFromDictionnary(_context->metadata, res);

        // HACK for OGG files
        // If we did not find tags, search metadata in streams
        if (res.empty())
        {
            for (std::size_t i{}; i < _context->nb_streams; ++i)
            {
                getMetaDataFromDictionnary(_context->streams[i]->metadata, res);

                if (!res.empty())
                    break;
            }
        }

        return res;
    }

    std::vector<StreamInfo> AudioFile::getStreamInfo() const
    {
        std::vector<StreamInfo> res;

        for (std::size_t i{}; i < _context->nb_streams; ++i)
        {
            std::optional<StreamInfo> streamInfo{ getStreamInfo(i) };
            if (streamInfo)
                res.emplace_back(std::move(*streamInfo));
        }

        return res;
    }

    std::optional<std::size_t> AudioFile::getBestStreamIndex() const
    {
        int res = ::av_find_best_stream(_context,
            AVMEDIA_TYPE_AUDIO,
            -1,	// Auto
            -1,	// Auto
            NULL,
            0);

        if (res < 0)
            return std::nullopt;

        return res;
    }

    std::optional<StreamInfo> AudioFile::getBestStreamInfo() const
    {
        std::optional<StreamInfo> res;

        std::optional<std::size_t> bestStreamIndex{ getBestStreamIndex() };
        if (bestStreamIndex)
            res = getStreamInfo(*bestStreamIndex);

        return res;
    }

    bool AudioFile::hasAttachedPictures() const
    {
        for (std::size_t i = 0; i < _context->nb_streams; ++i)
        {
            if (_context->streams[i]->disposition & AV_DISPOSITION_ATTACHED_PIC)
                return true;
        }

        return false;
    }

    void AudioFile::visitAttachedPictures(std::function<void(const Picture&)> func) const
    {
        static const std::unordered_map<int, std::string> codecMimeMap =
        {
            { AV_CODEC_ID_BMP, "image/x-bmp" },
            { AV_CODEC_ID_GIF, "image/gif" },
            { AV_CODEC_ID_MJPEG, "image/jpeg" },
            { AV_CODEC_ID_PNG, "image/png" },
            { AV_CODEC_ID_PNG, "image/x-png" },
            { AV_CODEC_ID_PPM, "image/x-portable-pixmap" },
        };

        for (std::size_t i = 0; i < _context->nb_streams; ++i)
        {
            AVStream* avstream = _context->streams[i];

            // Skip attached pics
            if (!(avstream->disposition & AV_DISPOSITION_ATTACHED_PIC))
                continue;

            if (avstream->codecpar == nullptr)
            {
                LMS_LOG(AV, ERROR, "Skipping stream " << i << " since no codecpar is set");
                continue;
            }

            Picture picture;

            auto itMime = codecMimeMap.find(avstream->codecpar->codec_id);
            if (itMime != codecMimeMap.end())
            {
                picture.mimeType = itMime->second;
            }
            else
            {
                picture.mimeType = "application/octet-stream";
                LMS_LOG(AV, ERROR, "CODEC ID " << avstream->codecpar->codec_id << " not handled in mime type conversion");
            }

            const AVPacket& pkt{ avstream->attached_pic };

            picture.data = reinterpret_cast<const std::byte*>(pkt.data);
            picture.dataSize = pkt.size;

            func(picture);
        }
    }

    std::optional<StreamInfo> AudioFile::getStreamInfo(std::size_t streamIndex) const
    {
        std::optional<StreamInfo> res;

        AVStream* avstream{ _context->streams[streamIndex] };
        assert(avstream);

        if (avstream->disposition & AV_DISPOSITION_ATTACHED_PIC)
            return res;

        if (!avstream->codecpar)
        {
            LMS_LOG(AV, ERROR, "Skipping stream " << streamIndex << " since no codecpar is set");
            return res;
        }

        if (avstream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO)
            return res;

        res.emplace();
        res->index = streamIndex;
        res->bitrate = static_cast<std::size_t>(avstream->codecpar->bit_rate);
        res->bitsPerSample = static_cast<std::size_t>(avstream->codecpar->bits_per_coded_sample);
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(59, 24, 100)
        res->channelCount = static_cast<std::size_t>(avstream->codecpar->channels);
#else
        res->channelCount = static_cast<std::size_t>(avstream->codecpar->ch_layout.nb_channels);
#endif
        res->codec = avcodecToDecodingCodec(avstream->codecpar->codec_id);
        res->codecName = ::avcodec_get_name(avstream->codecpar->codec_id);
        assert(!res->codecName.empty()); // doc says it is never NULL
        res->sampleRate = static_cast<std::size_t>(avstream->codecpar->sample_rate);

        return res;
    }

    std::string_view getMimeType(const std::filesystem::path& fileExtension)
    {
        // List should be sync with the demuxers shipped in the lms's docker version
        // + the _audioFileExtensions in ScanSettings
        // std::filesystem::path does not seem to have std::hash specialization on freebsd
        static const std::unordered_map<std::string_view, std::string_view> entries
        {
            {".mp3",    "audio/mpeg"},
            {".ogg",    "audio/ogg"},
            {".oga",    "audio/ogg"},
            {".opus",   "audio/opus"},
            {".aac",    "audio/aac"},
            {".alac",   "audio/mp4"},
            {".m4a",    "audio/mp4"},
            {".m4b",    "audio/mp4"},
            {".flac",   "audio/flac"},
            {".webm",   "audio/webm"},
            {".wav",    "audio/x-wav"},
            {".wma",    "audio/x-ms-wma"},
            {".ape",    "audio/x-monkeys-audio"},
            {".mpc",    "audio/x-musepack"},
            {".shn",    "audio/x-shn"},
            {".aif",    "audio/x-aiff"},
            {".aiff",   "audio/x-aiff"},
            {".m3u",    "audio/x-mpegurl"},
            {".pls",    "audio/x-scpls"},
            {".dsf",    "audio/x-dsd"},
            {".wv",     "audio/x-wavpack"},
            {".wvp",    "audio/x-wavpack"},
            {".mka",    "audio/x-matroska"},
        };

        auto it{ entries.find(core::stringUtils::stringToLower(fileExtension.string())) };
        if (it == std::cend(entries))
            return "";

        return it->second;
    }
} // namespace lms::av
