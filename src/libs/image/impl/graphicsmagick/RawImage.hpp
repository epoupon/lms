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

#ifndef LMS_SUPPORT_IMAGE_GM
#error "Bad configuration"
#endif

#include <Magick++.h>

#include <cstddef>
#include <filesystem>

#include "image/IEncodedImage.hpp"
#include "image/IRawImage.hpp"

namespace Image::GraphicsMagick
{
	class RawImage : public IRawImage
	{
		public:
			RawImage(const std::byte* encodedData, std::size_t encodedDataSize);
			RawImage(const std::filesystem::path& path);

			void resize(ImageSize width) override;
			std::unique_ptr<IEncodedImage> encodeToJPEG(unsigned quality) const override;

		private:
			friend class JPEGImage;
			Magick::Image getMagickImage() const;

			Magick::Image _image;
	};
}

