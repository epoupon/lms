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
#include <deque>

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

		typedef	std::deque<unsigned char>	data_type;

		static void init();

		~AvConvTranscoder();

		AvConvTranscoder(const Parameters& parameters);

		data_type& getOutputData()  { return _data; }

		const Parameters&	getParameters(void) const { return _parameters; }

		// Process a bunch of input data
		void process(void);
		bool isComplete(void) const { return _isComplete;};

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

		data_type	_data;
		bool		_isComplete;
};

} // naspace Transcode

#endif

