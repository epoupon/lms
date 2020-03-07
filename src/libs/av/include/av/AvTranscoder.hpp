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

#include <chrono>
#include <filesystem>
#include <optional>

#include <pstreams/pstream.h>

#include "AvTypes.hpp"

namespace Av {



struct TranscodeParameters
{
	Encoding				encoding;
	std::size_t				bitrate {128000};
	std::optional<std::size_t>		stream; // Id of the stream to be transcoded (auto detect by default)
	std::optional<std::chrono::seconds>	offset;
	bool 					stripMetadata {true};
};

class Transcoder
{
	public:
		static void init();

		Transcoder(const std::filesystem::path& file, const TranscodeParameters& parameters);
		~Transcoder();

		Transcoder(const Transcoder&) = delete;
		Transcoder& operator=(const Transcoder&) = delete;
		Transcoder(Transcoder&&) = delete;
		Transcoder& operator=(Transcoder&&) = delete;

		bool			start();
		const std::string&	getOutputMimeType() const { return _outputMimeType; }
		void			process(std::vector<unsigned char>& output, std::size_t maxSize);
		bool			isComplete(void) const { return _isComplete; }

		const TranscodeParameters& getParameters() const { return _parameters; }

	private:
		const std::filesystem::path	_filePath;
		const TranscodeParameters	_parameters;

		std::shared_ptr<redi::ipstream>	_child;

		bool			_isComplete {};
		std::size_t		_total {};
		const std::size_t	_id {};
		std::string		_outputMimeType;
};

} // namespace Av

