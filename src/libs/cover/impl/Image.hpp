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

#include <Magick++.h>

#include "utils/Exception.hpp"

namespace CoverArt
{
	void init(const std::filesystem::path& path);

	// internal use only
	class ImageException : public LmsException
	{
		public:
			using LmsException::LmsException;
	};

	class EncodedImage
	{
		public:
			EncodedImage() = default;
			EncodedImage(const std::byte* data, std::size_t dataSize);

			const std::byte* getData() const;
			std::size_t getDataSize() const;

		private:
			friend class RawImage;
			EncodedImage(Magick::Blob blob);

			Magick::Blob _blob;
	};

	class RawImage
	{
		public:
			RawImage(const std::filesystem::path& p);
			RawImage(const EncodedImage& encodedImage);

			// Operations
			void scale(std::size_t width);

			// output
			EncodedImage encode() const;

		private:
			Magick::Image _image;
	};

} // namespace CoverArt

