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

#ifndef AVCONV_TRANSCODER_HPP
#define AVCONV_TRANSCODER_HPP

#include <memory>
#include <iostream>
#include <vector>

#include <boost/iostreams/stream.hpp>
#include <boost/process.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>

#include "Parameters.hpp"

namespace Transcode
{

class AvConvTranscoder
{
	public:
		static void init();

		~AvConvTranscoder();

		AvConvTranscoder(const Parameters& parameters);

		const Parameters&	getParameters(void) const { return _parameters; }

		// Get a bunch of input data
		// Place it at the end of the parameter, no more that maxSize bytes
		void process(std::vector<unsigned char>& output, std::size_t maxSize);

		bool isComplete(void) const { return _isComplete;};
		std::size_t getOutputBytes(void) const { return _outputBytes; }

	private:

		typedef std::shared_ptr<boost::process::child> ChildPtr;

		void waitChild();
		void killChild();

		const Parameters	_parameters;

		static boost::mutex	_mutex;

		boost::process::pipe _outputPipe;
		boost::iostreams::file_descriptor_source _source;
		boost::iostreams::stream_buffer<boost::iostreams::file_descriptor_source> _is;
		std::istream		_in;

		std::shared_ptr<boost::process::child>	_child;

		static boost::filesystem::path _avConvPath;

		bool		_isComplete;
		std::size_t	_outputBytes;	// Bytes produced so far
};

} // naspace Transcode

#endif

