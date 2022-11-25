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

#include "utils/Logger.hpp"
#include "utils/String.hpp"

namespace Av {

static std::string averror_to_string(int error)
{
	std::array<char, 128> buf = {0};

	if (::av_strerror(error, buf.data(), buf.size()) == 0)
		return &buf[0];
	else
		return "Unknown error";
}

class AudioFileException : public Av::Exception
{
	public:
		AudioFileException(int avError)
			: Av::Exception {"AudioFileException: " + averror_to_string(avError)}
		{}
};

std::unique_ptr<IAudioFile>
parseAudioFile(const std::filesystem::path& p)
{
	return std::make_unique<AudioFile>(p);
}

AudioFile::AudioFile(const std::filesystem::path& p)
: _p {p}
{
	int error {avformat_open_input(&_context, _p.string().c_str(), nullptr, nullptr)};
	if (error < 0)
	{
		LMS_LOG(AV, ERROR) << "Cannot open " << _p.string() << ": " << averror_to_string(error);
		throw AudioFileException {error};
	}

	error = avformat_find_stream_info(_context, nullptr);
	if (error < 0)
	{
		LMS_LOG(AV, ERROR) << "Cannot find stream information on " << _p.string() << ": " << averror_to_string(error);
		avformat_close_input(&_context);
		throw AudioFileException {error};
	}
}

AudioFile::~AudioFile()
{
	avformat_close_input(&_context);
}

const std::filesystem::path&
AudioFile::getPath() const
{
	return _p;
}

std::chrono::milliseconds
AudioFile::getDuration() const
{
	if (_context->duration == AV_NOPTS_VALUE)
		return std::chrono::milliseconds {0}; // TODO estimate

	return std::chrono::milliseconds {_context->duration / AV_TIME_BASE * 1000};
}

void
getMetaDataFromDictionnary(AVDictionary* dictionnary, AudioFile::MetadataMap& res)
{
	if (!dictionnary)
		return;

	AVDictionaryEntry *tag = NULL;
	while ((tag = ::av_dict_get(dictionnary, "", tag, AV_DICT_IGNORE_SUFFIX)))
	{
		res[StringUtils::stringToUpper(tag->key)] = tag->value;
	}
}

AudioFile::MetadataMap
AudioFile::getMetaData() const
{
	MetadataMap res;

	getMetaDataFromDictionnary(_context->metadata, res);

	// HACK for OGG files
	// If we did not find tags, search metadata in streams
	if (res.empty())
	{
		for (std::size_t i {}; i < _context->nb_streams; ++i)
		{
			getMetaDataFromDictionnary(_context->streams[i]->metadata, res);

			if (!res.empty())
				break;
		}
	}

	return res;
}

std::vector<StreamInfo>
AudioFile::getStreamInfo() const
{
	std::vector<StreamInfo> res;

	for (std::size_t i {}; i < _context->nb_streams; ++i)
	{
		std::optional<StreamInfo> streamInfo {getStreamInfo(i)};
		if (streamInfo)
			res.emplace_back(std::move(*streamInfo));
	}

	return res;
}

std::optional<std::size_t>
AudioFile::getBestStreamIndex() const
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

std::optional<StreamInfo>
AudioFile::getBestStreamInfo() const
{
	std::optional<StreamInfo> res;

	std::optional<std::size_t> bestStreamIndex {getBestStreamIndex()};
	if (bestStreamIndex)
		res = getStreamInfo(*bestStreamIndex);

	return res;
}

bool
AudioFile::hasAttachedPictures() const
{
	for (std::size_t i = 0; i < _context->nb_streams; ++i)
	{
		if (_context->streams[i]->disposition & AV_DISPOSITION_ATTACHED_PIC)
			return true;
	}

	return false;
}

void
AudioFile::visitAttachedPictures(std::function<void(const Picture&)> func) const
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
		AVStream *avstream = _context->streams[i];

		// Skip attached pics
		if (!(avstream->disposition & AV_DISPOSITION_ATTACHED_PIC))
			continue;

		if (avstream->codecpar == nullptr)
		{
			LMS_LOG(AV, ERROR) << "Skipping stream " << i << " since no codecpar is set";
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
			LMS_LOG(AV, ERROR) << "CODEC ID " << avstream->codecpar->codec_id << " not handled in mime type conversion";
		}

		const AVPacket& pkt {avstream->attached_pic};

		picture.data = reinterpret_cast<const std::byte*>(pkt.data);
		picture.dataSize = pkt.size;

		func(picture);
	}
}

std::optional<StreamInfo>
AudioFile::getStreamInfo(std::size_t streamIndex) const
{
	std::optional<StreamInfo> res;

	AVStream* avstream { _context->streams[streamIndex]};
	assert(avstream);

	if (avstream->disposition & AV_DISPOSITION_ATTACHED_PIC)
		return res;

	if (!avstream->codecpar)
	{
		LMS_LOG(AV, ERROR) << "Skipping stream " << streamIndex << " since no codecpar is set";
		return res;
	}

	if (avstream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO)
		return res;

	res.emplace();
	res->index = streamIndex;
	res->bitrate = static_cast<std::size_t>(avstream->codecpar->bit_rate);
	res->codec = ::avcodec_get_name(avstream->codecpar->codec_id);
	assert(!res->codec.empty());

	return res;
}

std::optional<AudioFileFormat>
guessAudioFileFormat(const std::filesystem::path& file)
{
	const AVOutputFormat* format {::av_guess_format(NULL, file.string().c_str(), NULL)};
	if (!format || !format->name)
		return {};

	LMS_LOG(AV, DEBUG) << "File '" << file.string() << "', formats = '" << format->name << "'";

	auto formats {StringUtils::splitString(format->name, ",")};
	if (formats.size() > 1)
		LMS_LOG(AV, INFO) << "File '" << file.string() << "' reported several formats: '" << format->name << "'";

	std::vector<std::string_view> mimeTypes;
	if (format->mime_type)
		mimeTypes = StringUtils::splitString(format->mime_type, ",");

	if (mimeTypes.empty())
		LMS_LOG(AV, INFO) << "File '" << file.string() << "', no mime type found!";
	else if (mimeTypes.size() > 1)
		LMS_LOG(AV, INFO) << "File '" << file.string() << "' reported several mime types: '" << format->mime_type << "'";

	AudioFileFormat res;
	res.format = formats.front();
	res.mimeType = mimeTypes.empty() ? "application/octet-stream" : mimeTypes.front();

	return res;
}

} // namespace Av
