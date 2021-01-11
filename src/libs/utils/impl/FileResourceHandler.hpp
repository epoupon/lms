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
#include "utils/IResourceHandler.hpp"

class FileResourceHandler final : public IResourceHandler
{
	public:
		FileResourceHandler(const std::filesystem::path& filePath);

	private:

		Wt::Http::ResponseContinuation* processRequest(const Wt::Http::Request& request, Wt::Http::Response& response) override;

		static constexpr std::size_t _chunkSize {65536};

		std::filesystem::path	_path;
		::uint64_t		_beyondLastByte {};
		::uint64_t		_offset {};
		bool			_isFinished {};

};

