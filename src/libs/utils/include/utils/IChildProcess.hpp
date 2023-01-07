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

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "utils/Exception.hpp"

class ChildProcessException : public LmsException
{
	public:
		using LmsException::LmsException;
};

class IChildProcess
{
	public:
		using Args = std::vector<std::string>;

		virtual ~IChildProcess() = default;

		enum class ReadResult
		{
			Success,
			Error,
			EndOfFile,
		};

		using ReadCallback = std::function<void(ReadResult, std::size_t)>;
		virtual void		asyncRead(std::byte* data, std::size_t bufferSize, ReadCallback callback) = 0;

		virtual std::size_t	readSome(std::byte* data, std::size_t bufferSize) = 0;
		virtual bool		finished() const = 0;
};

