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

#include "image/IEncodedImage.hpp"

namespace Image
{
	class IRawImage
	{
		public:
			virtual ~IRawImage() = default;
			virtual void resize(ImageSize width) = 0;
			virtual std::unique_ptr<IEncodedImage> encodeToJPEG(unsigned quality) const = 0;
	};

	void init(const std::filesystem::path& path);
	std::unique_ptr<IRawImage> decodeImage(const std::byte* encodedData, std::size_t encodedDataSize);
	std::unique_ptr<IRawImage> decodeImage(const std::filesystem::path& path);
}

