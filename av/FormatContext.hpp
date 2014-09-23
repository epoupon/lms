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

#ifndef FORMAT_CONTEXT_HPP
#define FORMAT_CONTEXT_HPP

#include <boost/filesystem.hpp>

#include "Common.hpp"

namespace Av
{

class FormatContext
{
	public:
		FormatContext();
		~FormatContext();


	protected:
		void		 native(AVFormatContext* c) { _context = c;}
		AVFormatContext* native() { return _context; }
		const AVFormatContext* native() const { return _context; }

	private:

		AVFormatContext* _context;


};

} // namespace Av

#endif

