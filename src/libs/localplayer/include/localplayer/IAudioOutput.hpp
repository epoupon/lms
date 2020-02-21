/*
 * Copyright (C) 2020 Emeric Poupon
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

#include <functional>

#include "utils/Exception.hpp"

class AudioOutputException : public LmsException
{
	using LmsException::LmsException;
};

class IAudioOutput
{
	public:

		enum class Format
		{
			S16LE,
		};

		using SampleRate = std::size_t;
		using Volume = float;

		virtual ~IAudioOutput() = default;

		virtual Format		getFormat() const = 0;
		virtual SampleRate	getSampleRate() const = 0;
		virtual std::size_t	nbChannels() const = 0;

		virtual void start() = 0;
		virtual void stop() = 0;
		virtual void resume() = 0;
		virtual void pause() = 0;
		virtual void setVolume(Volume volume) = 0;
		virtual void flush() = 0;

		using OnCanWriteCallback = std::function<void(std::size_t nbBytes)>;
		virtual void setOnCanWriteCallback(OnCanWriteCallback func) = 0;
		virtual std::size_t getCanWriteBytes() = 0;
		virtual std::size_t write(const unsigned char* data, std::size_t nbBytes) = 0;
};

