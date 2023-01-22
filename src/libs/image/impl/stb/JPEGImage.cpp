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

#include "JPEGImage.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "image/Exception.hpp"
#include "RawImage.hpp"

namespace Image::STB
{
	JPEGImage::JPEGImage(const RawImage& rawImage, unsigned quality)
	{
		auto writeCb {[](void* ctx, void* writeData, int writeSize)
		{
			auto& output {*reinterpret_cast<std::vector<std::byte>*>(ctx)};
			const std::size_t currentOutputSize {output.size()};
			output.resize(currentOutputSize + writeSize);
			std::copy(reinterpret_cast<const std::byte*>(writeData), reinterpret_cast<const std::byte*>(writeData) + writeSize, output.data() + currentOutputSize);
		}};

		if (stbi_write_jpg_to_func(writeCb, &_data, rawImage.getWidth(), rawImage.getHeight(), 3, rawImage.getData(), quality) == 0)
		{
			_data.clear();
			throw ImageException {"Failed to export in jpeg format!"};
		}
	}

	const std::byte*
	JPEGImage::getData() const
	{
		if (_data.empty())
				return nullptr;

		return &_data.front();
	}

	std::size_t
	JPEGImage::getDataSize() const
	{
		return _data.size();
	}
}
