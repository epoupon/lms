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


namespace CoverArt
{

class CoverArt
{
	public:
		typedef std::vector<unsigned char>	data_type;

		CoverArt();
		CoverArt(const std::string& mime, const data_type& data) : _mimeType(mime), _data(data) {}

		const std::string&	getMimeType() const { return _mimeType; }
		const data_type&	getData() const { return _data; }

		void	setMimeType(const std::string& mimeType) { _mimeType = mimeType;}
		void	setData(const data_type& data) { _data = data; }

		bool	scale(std::size_t size);

	private:

		std::string	_mimeType;
		data_type	_data;
};


} // namespace CoverArt

#endif

