/*
 * Copyright (C) 2013 Emeric Poupon
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

#include <stdexcept>
#include <iostream>

#include "logger/Logger.hpp"

#include "InputFormatContext.hpp"

namespace Av
{

InputFormatContext::InputFormatContext(const boost::filesystem::path& p)
: _path(p)
{
	AVFormatContext* context = nullptr;

	// The last three parameters specify the file format, buffer size and
	// format parameters. By simply specifying NULL or 0 we ask libavformat
	// to auto-detect the format and use a default buffer size.
	AvError error = avformat_open_input(&context, p.string().c_str(), nullptr, nullptr);
	if (error)
	{
		LMS_LOG(MOD_AV, SEV_ERROR) << "Cannot open '" << p.string() << "', avformat_open_input returned " << error;
		throw std::runtime_error("avformat_open_input failed: " + error.to_str());
	}

	native(context);

}

InputFormatContext::~InputFormatContext()
{
	AVFormatContext* context = native();
	avformat_close_input(&context);
}

std::vector<Stream>
InputFormatContext::getStreams(void)
{
	std::vector<Stream> res;

	for (std::size_t i = 0; i < native()->nb_streams; ++i) {
		res.push_back( Stream(native()->streams[i]));
	}

	return res;
}


bool
InputFormatContext::getBestStreamIdx(AVMediaType type, Stream::Idx& index)
{
	int res = av_find_best_stream(native(),
			type,
			-1,	// Auto
			-1,	// Auto
			NULL,
			0
			);

	AvError error(res);
	if (error) {
		LMS_LOG(MOD_AV, SEV_DEBUG) << "Cannot get best stream for type " << type << ": " << error;
		return false;
	}
	else {
		index = res;
		return true;
	}
}

void
InputFormatContext::findStreamInfo(void)
{
	AvError err = avformat_find_stream_info(native(), NULL);
	if (err) {
		LMS_LOG(MOD_AV, SEV_ERROR) << "Couldn't find stream information: " << err;
		throw std::runtime_error("av_find_stream_info failed!");
	}
}


std::size_t
InputFormatContext::getDurationSecs() const
{
	if (static_cast<int>(native()->duration) != AV_NOPTS_VALUE )
		return native()->duration / AV_TIME_BASE;
	else
		return 0;	// TODO, do something better?
}

Dictionary
InputFormatContext::getMetadata(void)
{
	return Dictionary(native()->metadata);

}

std::size_t
InputFormatContext::getNbPictures(void) const
{
	std::size_t res = 0;

	for (std::size_t i = 0; i < native()->nb_streams; ++i)
	{
		if (native()->streams[i]->disposition & AV_DISPOSITION_ATTACHED_PIC)
			++res;
	}
	return res;
}

void
InputFormatContext::getPictures(std::vector<Picture>& pictures) const
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

	for (std::size_t i = 0; i < native()->nb_streams; ++i)
	{
		Stream stream(native()->streams[i]);

		if (stream.hasAttachedPic())
		{
			Picture picture;

			auto itMime = codecMimeMap.find(stream.getCodecContext().getCodecId());
			if (itMime != codecMimeMap.end())
				picture.mimeType = itMime->second;
			else
				picture.mimeType = "application/octet-stream";

			LMS_LOG(MOD_AV, SEV_DEBUG) << "MIME set to '" << picture.mimeType << "'" << std::endl;

			AVPacket pkt = native()->streams[i]->attached_pic;

			std::copy(pkt.data, pkt.data + pkt.size, std::back_inserter(picture.data));

			pictures.push_back( picture );
		}
	}
}

} //namespace Av
