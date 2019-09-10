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
#include <map>
#include <mutex>
#include <optional>
#include <vector>

#include "database/Types.hpp"

#include "image/Image.hpp"

namespace Database {
	class Session;
}

namespace CoverArt {

class Grabber
{
	public:
		Grabber();
		Grabber(const Grabber&) = delete;
		Grabber& operator=(const Grabber&) = delete;
		Grabber(Grabber&&) = delete;
		Grabber& operator=(Grabber&&) = delete;

		void			setDefaultCover(const std::filesystem::path& defaultCoverPath);

		std::vector<uint8_t>	getFromTrack(Database::Session& dbSession, Database::IdType trackId, Image::Format format, std::size_t size);
		std::vector<uint8_t>	getFromRelease(Database::Session& dbSession, Database::IdType releaseId, Image::Format format, std::size_t size);

	private:

		Image::Image		getFromTrack(Database::Session& dbSession, Database::IdType trackId, std::size_t size);
		Image::Image		getFromRelease(Database::Session& dbSession, Database::IdType releaseId, std::size_t size);

		std::optional<Image::Image>		getFromTrack(const std::filesystem::path& path) const;
		std::vector<std::filesystem::path>	getCoverPaths(const std::filesystem::path& directoryPath) const;
		std::optional<Image::Image>		getFromDirectory(const std::filesystem::path& path) const;

		Image::Image	getDefaultCover(std::size_t size);

		Image::Image _defaultCover;

		std::mutex _mutex;
		std::map<std::size_t /* size */, Image::Image> _defaultCovers;

		std::vector<std::filesystem::path> _fileExtensions
			= {".jpg", ".jpeg", ".png", ".bmp"}; // TODO parametrize

		std::size_t _maxFileSize = 5000000;

		std::vector<std::filesystem::path> _preferredFileNames
			= {"cover", "front"}; // TODO parametrize
};

} // namespace CoverArt

