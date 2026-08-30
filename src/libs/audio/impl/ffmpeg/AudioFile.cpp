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

#include <cstdio>
#include <unordered_map>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "core/String.hpp"

#include "Exception.hpp"
#include "Utils.hpp"

namespace lms::audio::ffmpeg
{
    namespace
    {
        void extractMetaDataFromDictionnary(AVDictionary* dictionnary, AudioFile::MetadataMap& res)
        {
            if (!dictionnary)
                return;

            const AVDictionaryEntry* tag{ NULL };
            while ((tag = av_dict_iterate(dictionnary, tag)))
                res[core::stringUtils::stringToUpper(tag->key)] = tag->value;
        }

    } // namespace

    AudioFile::AudioFile(const std::filesystem::path& p)
        : _p{ p }
    {
        LMS_SCOPED_TRACE_DETAILED("MetaData", "FFmpegParseFile");

        assert(utils::isInit());

        int error{ avformat_open_input(&_context, _p.c_str(), nullptr, nullptr) };
        if (error < 0)
        {
            LMS_LOG(AUDIO, ERROR, "Cannot open " << _p << ": " << utils::averrorToString(error));
            throw FFmpegException{ "Cannot open '" + _p.string() + "'", error };
        }

        error = avformat_find_stream_info(_context, nullptr);
        if (error < 0)
        {
            LMS_LOG(AUDIO, ERROR, "Cannot find stream information in " << _p << ": " << utils::averrorToString(error));
            avformat_close_input(&_context);
            throw FFmpegException{ "Cannot find stream information in '" + _p.string() + "'", error };
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

        info.container = utils::containerFromFormatName(_context->iformat->name);
        info.containerName = _context->iformat->name;

        if (_context->bit_rate > 0)
            info.bitrate = _context->bit_rate;
        info.duration = std::chrono::milliseconds{ _context->duration == AV_NOPTS_VALUE ? 0 : _context->duration * 1'000 / AV_TIME_BASE };

        return info;
    }

    AudioFile::MetadataMap AudioFile::extractMetaData() const
    {
        MetadataMap res;

        extractMetaDataFromDictionnary(_context->metadata, res);

        // HACK for OGG files
        // If we did not find tags, search metadata in streams
        if (res.empty())
        {
            for (std::size_t i{}; i < _context->nb_streams; ++i)
            {
                extractMetaDataFromDictionnary(_context->streams[i]->metadata, res);

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
                                        -1, // Auto
                                        -1, // Auto
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
        for (std::size_t i{}; i < _context->nb_streams; ++i)
        {
            if (_context->streams[i]->disposition & AV_DISPOSITION_ATTACHED_PIC)
                return true;
        }

        return false;
    }

    void AudioFile::visitAttachedPictures(std::function<void(const PictureView&, const MetadataMap&)> func) const
    {
        static const std::unordered_map<int, std::string> codecMimeMap{
            { AV_CODEC_ID_BMP, "image/bmp" },
            { AV_CODEC_ID_GIF, "image/gif" },
            { AV_CODEC_ID_MJPEG, "image/jpeg" },
            { AV_CODEC_ID_PNG, "image/png" },
            { AV_CODEC_ID_PPM, "image/x-portable-pixmap" },
        };

        for (std::size_t i{}; i < _context->nb_streams; ++i)
        {
            AVStream* avstream = _context->streams[i];

            // Skip attached pics
            if (!(avstream->disposition & AV_DISPOSITION_ATTACHED_PIC))
                continue;

            if (avstream->codecpar == nullptr)
            {
                LMS_LOG(AUDIO, WARNING, "Skipping stream " << i << " since no codecpar is set");
                continue;
            }

            MetadataMap metadata;
            extractMetaDataFromDictionnary(avstream->metadata, metadata);

            PictureView picture;

            auto itMime = codecMimeMap.find(avstream->codecpar->codec_id);
            if (itMime != codecMimeMap.end())
            {
                picture.mimeType = itMime->second;
            }
            else
            {
                picture.mimeType = "application/octet-stream";
                LMS_LOG(AUDIO, WARNING, "AVCodecID" << avstream->codecpar->codec_id << " (" << ::avcodec_get_name(avstream->codecpar->codec_id) << ") not handled in mime type conversion");
            }

            const ::AVPacket& pkt{ avstream->attached_pic };

            picture.data = std::span{ reinterpret_cast<const std::byte*>(pkt.data), static_cast<std::size_t>(pkt.size) };
            func(picture, metadata);
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
            LMS_LOG(AUDIO, WARNING, "Skipping stream " << streamIndex << " since no codecpar is set");
            return res;
        }

        if (avstream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO)
            return res;

        res.emplace();

        res->index = streamIndex;
        res->codec = utils::codecFromAVCodecId(avstream->codecpar->codec_id);
        res->codecName = ::avcodec_get_name(avstream->codecpar->codec_id);

        if (avstream->codecpar->bit_rate)
            res->bitrate = static_cast<std::size_t>(avstream->codecpar->bit_rate);
        if (avstream->codecpar->bits_per_coded_sample)
            res->bitsPerSample = static_cast<std::size_t>(avstream->codecpar->bits_per_coded_sample);
        else if (avstream->codecpar->bits_per_raw_sample)
            res->bitsPerSample = static_cast<std::size_t>(avstream->codecpar->bits_per_raw_sample);

        if (avstream->codecpar->ch_layout.nb_channels)
            res->channelCount = static_cast<std::size_t>(avstream->codecpar->ch_layout.nb_channels);
        assert(!res->codecName.empty()); // doc says it is never NULL
        if (avstream->codecpar->sample_rate)
            res->sampleRate = static_cast<std::size_t>(avstream->codecpar->sample_rate);

        return res;
    }
} // namespace lms::audio::ffmpeg