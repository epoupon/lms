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

#ifndef TRANSCODE_STREAM_HPP
#define TRANSCODE_STREAM_HPP

#include <string>

namespace Transcode
{

class Stream
{

	public:

		enum Type
		{
			Audio,
			Video,
			Subtitle,
		};

		typedef std::size_t  Id;

		Stream(Id id, Type type, std::size_t bitrate, const std::string& lang, const std::string& desc)
			: _id(id), _type(type), _bitrate(bitrate), _language(lang), _desc(desc) {}

		// Accessors
		Id			getId() const		{ return _id;}
		Type			getType() const		{ return _type;}
		std::size_t		getBitrate() const	{ return _bitrate;}
		const std::string&	getLanguage() const	{ return _language;}
		const std::string&	getDesc() const		{ return _desc;}

	private:

		Id		_id;
		Type		_type;
		std::size_t	_bitrate;
		std::string	_language;
		std::string	_desc;

};


} // namespace Transcode

#endif

