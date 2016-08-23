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

#include <array>

#include "utils/Logger.hpp"

#include "AvInfo.hpp"

namespace Av {

static std::string streamType_to_string(Stream::Type type)
{
	switch (type)
	{
		case Stream::Type::Audio: return "audio";
		case Stream::Type::Video: return "video";
		case Stream::Type::Subtitle: return "subtitle";
	}

	return "unknown";
}

static std::string averror_to_string(int error)
{
	std::array<char, 128> buf = {0};

	if (av_strerror(error, buf.data(), buf.size()) == 0)
		return std::string(&buf[0]);
	else
		return "Unknown error";
}


void AvInit()
{
       /* register all the codecs */
       avcodec_register_all();
       av_register_all();
       LMS_LOG(AV, INFO) << "avcodec version = " << avcodec_version();
}




MediaFile::MediaFile(const boost::filesystem::path& p)
: _p(p), _context(nullptr)
{
}

MediaFile::~MediaFile()
{
	if (_context != nullptr)
		avformat_close_input(&_context);
}

bool
MediaFile::open(void)
{
	if (_context != nullptr)
		throw std::logic_error("inputfile '" + _p.string() + "' already open");

	int error = avformat_open_input(&_context, _p.string().c_str(), nullptr, nullptr);
	if (error < 0)
	{
		LMS_LOG(AV, ERROR) << "Cannot open '" << _p.string() << "': " << averror_to_string(error);
		return false;
	}

	return true;
}

bool
MediaFile::scan(void)
{
	if (_context == nullptr)
		throw std::logic_error("inputfile '" + _p.string() + "' not open");

	int error = avformat_find_stream_info(_context, nullptr);
	if (error < 0)
	{
		LMS_LOG(AV, ERROR) << "Cannot find stream information on '" << _p.string() << "': " << averror_to_string(error);
		return false;
	}

	return true;
}

boost::posix_time::time_duration
MediaFile::getDuration() const
{
	if (_context == nullptr)
		throw std::logic_error("inputfile '" + _p.string() + "' not open");

	if (static_cast<int>(_context->duration) != AV_NOPTS_VALUE )
		return boost::posix_time::seconds(_context->duration / AV_TIME_BASE);
	else
		return boost::posix_time::seconds(0);	// TODO, do something better?
}

void
getMetaDataFromDictionnary(AVDictionary* dictionnary, std::map<std::string, std::string>& res)
{
	if (!dictionnary)
		return;

	AVDictionaryEntry *tag = NULL;
	while ((tag = av_dict_get(dictionnary, "", tag, AV_DICT_IGNORE_SUFFIX)))
	{
		res.insert( std::make_pair(tag->key, tag->value));
	}
}

std::map<std::string, std::string>
MediaFile::getMetaData(void)
{
	if (_context == nullptr)
		throw std::logic_error("inputfile '" + _p.string() + "' not open");

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

std::vector<Stream>
MediaFile::getStreams(Stream::Type type) const
{
	if (_context == nullptr)
		throw std::logic_error("inputfile '" + _p.string() + "' not open");

	std::vector<Stream> res;

	for (std::size_t i = 0; i < _context->nb_streams; ++i)
	{
		AVStream*	avstream = _context->streams[i];

		// Skip attached pics
		if (avstream->disposition & AV_DISPOSITION_ATTACHED_PIC)
			continue;

		if (avstream->codec == nullptr)
		{
			LMS_LOG(AV, ERROR) << "Skipping stream " << i << " since no codec is set";
			continue;
		}

		if (type == Stream::Type::Audio && avstream->codec->codec_type != AVMEDIA_TYPE_AUDIO)
			continue;
		else if (type == Stream::Type::Video && avstream->codec->codec_type != AVMEDIA_TYPE_VIDEO)
			continue;
		else if (type == Stream::Type::Subtitle && avstream->codec->codec_type != AVMEDIA_TYPE_SUBTITLE)
			continue;

		Stream stream;

		stream.id = i; // or use stream->id ?
		stream.type = type;
		stream.bitrate = avstream->codec->bit_rate;

		{
			std::array<char, 256> buf = {0};
			avcodec_string(buf.data(), buf.size(), avstream->codec, 0);

			stream.desc = buf.data();
		}

		res.push_back(stream);
	}

	return res;
}

int
MediaFile::getBestStreamId(Stream::Type type) const
{
	if (_context == nullptr)
		throw std::logic_error("inputfile '" + _p.string() + "' not open");

	enum AVMediaType avMediaType;

	switch (type)
	{
		case Stream::Type::Audio: avMediaType = AVMEDIA_TYPE_AUDIO; break;
		case Stream::Type::Video: avMediaType = AVMEDIA_TYPE_VIDEO; break;
		case Stream::Type::Subtitle: avMediaType = AVMEDIA_TYPE_SUBTITLE; break;
		default:
			return -1;
	}

	int res = av_find_best_stream(_context,
		avMediaType,
		-1,	// Auto
		-1,	// Auto
		NULL,
		0);
	if (res < 0)
	{
		LMS_LOG(AV, ERROR) << "Cannot find best stream for type " << streamType_to_string(type);
		return -1;
	}

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
	if (_context == nullptr)
		throw std::logic_error("inputfile '" + _p.string() + "' not open");

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

		if (avstream->codec == nullptr)
		{
			LMS_LOG(AV, ERROR) << "Skipping stream " << i << " since no codec is set";
			continue;
		}

		Picture picture;

		auto itMime = codecMimeMap.find(avstream->codec->codec_id);
		if (itMime != codecMimeMap.end())
		{
			picture.mimeType = itMime->second;
		}
		else
		{
			picture.mimeType = "application/octet-stream";
			LMS_LOG(AV, ERROR) << "CODEC ID " << avstream->codec->codec_id << " not handled in mime type conversion";
		}

		AVPacket pkt = avstream->attached_pic;

		std::copy(pkt.data, pkt.data + pkt.size, std::back_inserter(picture.data));

		pictures.push_back( picture );

		if (pictures.size() >= nbMaxPictures)
			break;
	}

	return pictures;
}

} // namespace Av
