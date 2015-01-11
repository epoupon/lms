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


#include "logger/Logger.hpp"

#include "av/InputFormatContext.hpp"

#include "Parameters.hpp"

namespace Transcode
{

Parameters::Parameters(const InputMediaFile& inputMediaFile,
		const Format& outputFormat)
	:
_mediaFile(inputMediaFile),
_outputFormat(outputFormat)
{
	// By default, select the best stream indexes
	_inputStreams = _mediaFile.getBestStreams();

}

std::size_t
Parameters::setBitrate(Stream::Type type, std::size_t bitrate)
{
	// Limit the output bitrate to the input bitrate
	if (_inputStreams.find(type) != _inputStreams.end())
	{
		const Transcode::Stream& stream = _mediaFile.getStream( _inputStreams[type] );

		LMS_LOG(MOD_TRANSCODE, SEV_DEBUG) << "Stream bitrate = " << stream.getBitrate();

		if (bitrate > stream.getBitrate())
		{
			LMS_LOG(MOD_TRANSCODE, SEV_INFO) << "Setting bitrate for stream idx " << _inputStreams[type] << " to input bitrate (" << stream.getBitrate() << ")";
			_outputBitrate[type] = stream.getBitrate();
		}
		else
		{
			LMS_LOG(MOD_TRANSCODE, SEV_DEBUG) << "Setting bitrate for stream idx " << _inputStreams[type] << " to " << bitrate;
			_outputBitrate[type] = bitrate;
		}
		return _outputBitrate[type];
	}
	else
	{
		LMS_LOG(MOD_TRANSCODE, SEV_DEBUG) << "Cannot find stream type " << type;
		return 0;
	}
}

std::size_t
Parameters::getOutputBitrate(Stream::Type type) const
{
	std::map<Stream::Type, std::size_t>::const_iterator it = _outputBitrate.find(type);

	if (it != _inputStreams.end())
	{
		return it->second;
	}
	else
	{
		LMS_LOG(MOD_TRANSCODE, SEV_DEBUG) << "output bitrate not set for type " << type;
		return 0;
	}
}

} // namespace Transcode

