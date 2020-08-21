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

#include "av/AvInfo.hpp"

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

	if (av_strerror(error, buf.data(), buf.size()) == 0)
		return std::string(&buf[0]);
	else
		return "Unknown error";
}

MediaFileException::MediaFileException(int avError)
: AvException("MediaFileException: " + averror_to_string(avError))
{
}


MediaFile::MediaFile(const std::filesystem::path& p)
: _p {p}
{
	int error = avformat_open_input(&_context, _p.string().c_str(), nullptr, nullptr);
	if (error < 0)
	{
		LMS_LOG(AV, ERROR) << "Cannot open " << _p.string() << ": " << averror_to_string(error);
		throw MediaFileException(error);
	}

	error = avformat_find_stream_info(_context, nullptr);
	if (error < 0)
	{
		LMS_LOG(AV, ERROR) << "Cannot find stream information on " << _p.string() << ": " << averror_to_string(error);
		avformat_close_input(&_context);
		throw MediaFileException(error);
	}
}

MediaFile::~MediaFile()
{
	avformat_close_input(&_context);
}

std::string
MediaFile::getFormatName() const
{
	return _context->iformat->name;
}

std::chrono::milliseconds
MediaFile::getDuration() const
{
	if (_context->duration == AV_NOPTS_VALUE)
		return std::chrono::milliseconds(0); // TODO estimate

	return std::chrono::milliseconds(_context->duration / AV_TIME_BASE * 1000);
}

void
getMetaDataFromDictionnary(AVDictionary* dictionnary, std::map<std::string, std::string>& res)
{
	if (!dictionnary)
		return;

	AVDictionaryEntry *tag = NULL;
	while ((tag = av_dict_get(dictionnary, "", tag, AV_DICT_IGNORE_SUFFIX)))
	{
		res[StringUtils::stringToUpper(tag->key)] = tag->value;
	}
}

std::map<std::string, std::string>
MediaFile::getMetaData(void)
{
	std::map<std::string, std::string> res;

	getMetaDataFromDictionnary(_context->metadata, res);

	// HACK for OGG files
	// If we did not find tags, search metadata in streams
	if (res.empty())
	{
		for (std::size_t i = 0; i < _context->nb_streams; ++i)
		{
			getMetaDataFromDictionnary(_context->streams[i]->metadata, res);

			if (!res.empty())
				break;
		}
	}

	return res;
}

std::vector<StreamInfo>
MediaFile::getStreamInfo() const
{
	std::vector<StreamInfo> res;

	for (std::size_t i {}; i < _context->nb_streams; ++i)
	{
		AVStream* avstream { _context->streams[i]};

		// Skip attached pics
		if (avstream->disposition & AV_DISPOSITION_ATTACHED_PIC)
			continue;

		if (!avstream->codecpar)
		{
			LMS_LOG(AV, ERROR) << "Skipping stream " << i << " since no codecpar is set";
			continue;
		}

		if (avstream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO)
			continue;

		res.push_back( {i, static_cast<std::size_t>(avstream->codecpar->bit_rate)} );
	}

	return res;
}

std::optional<std::size_t>
MediaFile::getBestStream() const
{
	int res = av_find_best_stream(_context,
		AVMEDIA_TYPE_AUDIO,
		-1,	// Auto
		-1,	// Auto
		NULL,
		0);

	if (res < 0)
		return std::nullopt;

	return res;
}

bool
MediaFile::hasAttachedPictures(void) const
{
	for (std::size_t i = 0; i < _context->nb_streams; ++i)
	{
		if (_context->streams[i]->disposition & AV_DISPOSITION_ATTACHED_PIC)
			return true;
	}

	return false;
}

std::vector<Picture>
MediaFile::getAttachedPictures(std::size_t nbMaxPictures) const
{
	static const std::map<int, std::string> codecMimeMap =
	{
		{ AV_CODEC_ID_BMP, "image/x-bmp" },
		{ AV_CODEC_ID_GIF, "image/gif" },
		{ AV_CODEC_ID_MJPEG, "image/jpeg" },
		{ AV_CODEC_ID_PNG, "image/png" },
		{ AV_CODEC_ID_PNG, "image/x-png" },
		{ AV_CODEC_ID_PPM, "image/x-portable-pixmap" },
	};

	std::vector<Picture> pictures;

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

		std::copy(pkt.data, pkt.data + pkt.size, std::back_inserter(picture.data));

		pictures.push_back( picture );

		if (pictures.size() >= nbMaxPictures)
			break;
	}

	return pictures;
}

std::optional<MediaFileFormat>
guessMediaFileFormat(const std::filesystem::path& file)
{
	AVOutputFormat* format {av_guess_format(NULL,file.string().c_str(),NULL)};
	if (!format || !format->name)
		return {};

	LMS_LOG(AV, DEBUG) << "File '" << file.string() << "', formats = '" << format->name << "'";

	auto formats {StringUtils::splitString(format->name, ",")};
	if (formats.size() > 1)
		LMS_LOG(AV, INFO) << "File '" << file.string() << "' reported several formats: '" << format->name << "'";

	std::vector<std::string> mimeTypes;
	if (format->mime_type)
		mimeTypes = StringUtils::splitString(format->mime_type, ",");

	if (mimeTypes.empty())
		LMS_LOG(AV, INFO) << "File '" << file.string() << "', no mime type found!";
	else if (mimeTypes.size() > 1)
		LMS_LOG(AV, INFO) << "File '" << file.string() << "' reported several mime types: '" << format->mime_type << "'";

	MediaFileFormat res;
	res.format = formats.front();
	res.mimeType = mimeTypes.empty() ? "application/octet-stream" : mimeTypes.front();

	return res;
}

} // namespace Av
