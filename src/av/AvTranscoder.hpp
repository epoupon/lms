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

#include <pstreams/pstream.h>

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>

#include "AvInfo.hpp"
#include "AvTypes.hpp"

namespace Av {



struct TranscodeParameters
{
	Encoding	encoding {Encoding::MP3};
	std::size_t	bitrate {128000};
	boost::optional<std::size_t> stream; // Id of the stream to be transcoded (auto detect by default)
	boost::optional<std::chrono::seconds> offset;
	bool stripMetadata {true};
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
		bool isComplete(void) const { return _isComplete; }

		const TranscodeParameters& getParameters() const { return _parameters; }

	private:
		Transcoder();

		boost::filesystem::path	_filePath;
		TranscodeParameters	_parameters;

		std::shared_ptr<redi::ipstream>	_child;

		bool			_isComplete = false;
		std::size_t		_total = 0;
		std::size_t		_id;
};

} // namespace Av

