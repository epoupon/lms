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
#include <vector>

#include "database/Types.hpp"
#include "cover/CoverArt.hpp"

namespace Database {
	class Session;
}

namespace CoverArt {

class IGrabber
{
	public:
		virtual ~IGrabber() = default;

		virtual void			setDefaultCover(const std::filesystem::path& defaultCoverPath) = 0;

		virtual std::vector<uint8_t>	getFromTrack(Database::Session& dbSession, Database::IdType trackId, Format format, std::size_t width) = 0;
		virtual std::vector<uint8_t>	getFromRelease(Database::Session& dbSession, Database::IdType releaseId, Format format, std::size_t width) = 0;

};

} // namespace CoverArt

