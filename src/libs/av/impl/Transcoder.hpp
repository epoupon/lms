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

#include <filesystem>
#include <functional>

#include "av/TranscodeParameters.hpp"
#include "av/Types.hpp"

class IChildProcess;

namespace Av
{
	class Transcoder
	{
		public:
			Transcoder(const InputFileParameters& inputFileParameters, const TranscodeParameters& transcodeParameters);
			~Transcoder();

			Transcoder(const Transcoder&) = delete;
			Transcoder& operator=(const Transcoder&) = delete;
			Transcoder(Transcoder&&) = delete;
			Transcoder& operator=(Transcoder&&) = delete;

			// non blocking calls
			using ReadCallback = std::function<void(std::size_t nbReadBytes)>;
			void			asyncRead(std::byte* buffer, std::size_t bufferSize, ReadCallback);
			std::size_t		readSome(std::byte* buffer, std::size_t bufferSize);

			const std::string&	getOutputMimeType() const { return _outputMimeType; }
			const TranscodeParameters& getParameters() const { return _transcodeParameters; }

			bool			finished() const;

		private:
			static void init();

			void start();

			const std::size_t			_debugId {};
			const InputFileParameters	_inputFileParameters;
			const TranscodeParameters	_transcodeParameters;

			std::unique_ptr<IChildProcess>	_childProcess;

			std::string		_outputMimeType;
	};

} // namespace Av

