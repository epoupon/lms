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

#ifndef TRANSCODE_PARAMETERS_HPP
#define TRANSCODE_PARAMETERS_HPP

#include <string>

#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "Format.hpp"

#include "InputMediaFile.hpp"

namespace Transcode
{


class Parameters {

	public:

		Parameters(const InputMediaFile& InputFile, const Format& outputFormat);

		// Modifiers
		void setOffset(boost::posix_time::time_duration offset)	{ _offset = offset; }	// Set input offset
		void setOutputFormat(const Format& format)		{ _outputFormat = format; }
		std::size_t setBitrate(Stream::Type type, std::size_t bitrate);

		// Manually select an input stream to output
		// There can be only one stream per each type (video, audio, subtitle)
		typedef std::map<Stream::Type, Stream::Id> StreamMap;
		void 					selectInputStream(Stream::Type type, Stream::Id id)	{ _inputStreams[type] = id; }
		const StreamMap& 			getInputStreams(void) const	{return _inputStreams;}

		//Accessors
		boost::posix_time::time_duration	getOffset(void) const {return _offset;}
		const Format&				getOutputFormat(void) const { return _outputFormat; }
		std::size_t				getOutputBitrate(Stream::Type type) const;
		const InputMediaFile&			getInputMediaFile(void) const { return _mediaFile;}
		InputMediaFile&				getInputMediaFile(void) { return _mediaFile;}


	private:

		InputMediaFile				_mediaFile;

		boost::posix_time::time_duration	_offset;		// start input offset

		Format					_outputFormat;		// OGA, OGV, etc.
		std::map<Stream::Type, std::size_t>	_outputBitrate;

		StreamMap				_inputStreams;

};

} // namespace Transcode

#endif

