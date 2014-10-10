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

#ifndef TRANSCODE_FORMAT_HPP
#define TRANSCODE_FORMAT_HPP

#include <vector>
#include <string>

namespace Transcode
{


class Format
{
	public:

		// Output format
		enum Encoding {
			OGA,
			OGV,
			MP3,
			WEBMA,
			WEBMV,
			FLV,
			M4A,
			M4V,
		};

		enum Type {
			Video,
			Audio,
		};

		// Utility
		static const Format&			get(Encoding encoding);
		static std::vector<Format>		get(Type type);

		Format(Encoding format, Type type, std::string mimeType, std::string desc);

		Encoding		getEncoding(void) const		{ return _encoding;}
		Type			getType(void) const		{ return _type;}
		const std::string&	getMimeType(void) const		{ return _mineType;}
		const std::string&	getDesc(void) const		{ return _desc;}

		bool	operator==(const Format& other) const;

	private:
		Encoding	_encoding;
		Type		_type;
		std::string	_mineType;
		std::string	_desc;

		static const std::vector<Format>	_supportedFormats;

};

} // namespace Transcode

#endif
