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
#include <memory>

#include "database/Types.hpp"
#include "cover/IEncodedImage.hpp"

namespace Database
{
	class Session;
}

namespace CoverArt
{
	class IGrabber
	{
		public:
			virtual ~IGrabber() = default;

			virtual std::shared_ptr<IEncodedImage>	getFromTrack(Database::Session& dbSession, Database::TrackId trackId, ImageSize width) = 0;
			virtual std::shared_ptr<IEncodedImage>	getFromRelease(Database::Session& dbSession, Database::ReleaseId releaseId, ImageSize width) = 0;

			virtual void flushCache() = 0;
	};

	std::unique_ptr<IGrabber> createGrabber(const std::filesystem::path& execPath,
			const std::filesystem::path& defaultCoverPath,
			std::size_t maxCacheEntries,
			std::size_t maxFileSize,
			unsigned jpegQuality);

} // namespace CoverArt

