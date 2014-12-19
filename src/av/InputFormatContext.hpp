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

#ifndef INPUT_FORMAT_CONTEXT_HPP
#define INPUT_FORMAT_CONTEXT_HPP

#include <vector>

#include <boost/filesystem.hpp>

#include "FormatContext.hpp"
#include "Stream.hpp"
#include "Dictionary.hpp"

namespace Av
{

class InputFormatContext : public FormatContext
{
	public:

		InputFormatContext(const boost::filesystem::path& p);
		~InputFormatContext();


		Dictionary	getMetadata(void);	// metadata access

		// Scan file
		void findStreamInfo();


		// Get attached pictures
		std::size_t		getNbPictures(void) const;
		void			getPictures(std::vector< std::vector<unsigned char> >& pictures) const;

		// Get the streams
		std::vector<Stream>	getStreams(void);

		bool 			getBestStreamIdx(enum AVMediaType type, Stream::Idx& idx);
		std::size_t		getDurationSecs() const;

	private:
		boost::filesystem::path _path;


};

} // namespace av

#endif

