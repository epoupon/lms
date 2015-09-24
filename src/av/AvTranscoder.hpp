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

#pragma once

#include <map>
#include <set>
#include <iostream>

#include <boost/iostreams/stream.hpp>
#include <boost/process.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>

#include <boost/date_time/posix_time/posix_time_types.hpp> //no i/o just types
#include <boost/filesystem/path.hpp>

#include "AvInfo.hpp"

namespace Av {


enum class Encoding
{
	OGA,
	OGV,
	MP3,
	WEBMA,
	WEBMV,
	FLA,
	FLV,
	M4A,
	M4V,
};

std::string encoding_to_mimetype(Encoding encoding);

class TranscodeParameters
{
	public:

		// Setters
		void setEncoding(Encoding encoding)			{ _encoding = encoding; }
		void setOffset(boost::posix_time::time_duration offset)	{_offset = offset; }
		void setBitrate(Stream::Type type, std::size_t bitrate)	{ _outputBitrate[type] = bitrate; }

		// Manually add the streams to be transcoded
		// If no stream is added, input streams are selected automatically
		void addStream(int inputStreamId)	{ _selectedStreams.insert(inputStreamId); }

		// Getters
		Encoding				getEncoding(void) const { return _encoding; }
		boost::posix_time::time_duration	getOffset(void) const { return _offset; }
		std::set<int>				getSelectedStreamIds(void) const { return _selectedStreams; }
		std::size_t				getBitrate(Stream::Type type) { return _outputBitrate[type]; }

	private:
		Encoding				_encoding = Encoding::MP3;
		boost::posix_time::time_duration	_offset = boost::posix_time::seconds(0);
		std::set<int>				_selectedStreams;
		std::map<Stream::Type, std::size_t>	_outputBitrate = { {Stream::Type::Audio, 0}, { Stream::Type::Video, 0}, { Stream::Type::Subtitle, 0} };
};


class Transcoder
{
	public:
		static void init();

		Transcoder(boost::filesystem::path file, TranscodeParameters parameters);
		~Transcoder();

		// non copyable
		Transcoder(const Transcoder&) = delete;
		Transcoder& operator=(const Transcoder&) = delete;

		bool start();
		void process(std::vector<unsigned char>& output, std::size_t maxSize);
		bool isComplete(void);

	private:

		Transcoder();

		boost::filesystem::path	_filePath;
		TranscodeParameters	_parameters;

		static boost::mutex     _mutex;

		boost::process::pipe _outputPipe;
		boost::iostreams::file_descriptor_source _source;
		boost::iostreams::stream_buffer<boost::iostreams::file_descriptor_source> _is;
		std::istream		_in;

		void waitChild();
		void killChild();

		std::shared_ptr<boost::process::child>	_child;

		static boost::filesystem::path _avConvPath;
		bool		_isComplete;

};

} // namespace Av

