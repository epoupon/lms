/*
 * Copyright (C) 2013 Emeric Poupon
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

#ifndef COVER_ART_HPP
#define COVER_ART_HPP

#include <vector>
#include <string>

#include <Magick++.h>

namespace CoverArt
{

enum class Format
{
	JPEG,
};

std::string format_to_mimeType(Format format);

class CoverArt
{
	public:
		static void init(const char *path);

		CoverArt() {}
		CoverArt(const std::vector<unsigned char>& rawData);

		// Operations on cover arts
		bool	scale(std::size_t size);

		// output
		void	getData(std::vector<unsigned char>& data, Format format) const;

	private:
		Magick::Image	_image;
};


} // namespace CoverArt

#endif

