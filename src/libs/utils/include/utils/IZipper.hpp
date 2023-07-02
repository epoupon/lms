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

#include <filesystem>
#include <memory>
#include <vector>

#include "Exception.hpp"

namespace Zip
{
	struct Entry
	{
		std::string fileName;
		std::filesystem::path filePath;
	};
	using EntryContainer = std::vector<Entry>;

	class Exception : public LmsException
	{
		using LmsException::LmsException;
	};

	class IZipper
	{
		public:
			virtual ~IZipper() = default;

			virtual std::uint64_t writeSome(std::ostream& output) = 0;
			virtual bool isComplete() const = 0;
			virtual void abort() = 0;
	};

	std::unique_ptr<IZipper> createArchiveZipper(const EntryContainer& entries);
} // namespace Zip

