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

#ifndef TRANSCODE_COMMON_HPP__
#define TRANSCODE_COMMON_HPP__

// Hack to properly iinclude libavcodec/avcodec.h...
//
#define __STDC_CONSTANT_MACROS
#ifdef _STDINT_H
#undef _STDINT_H
#endif
# include <stdint.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mathematics.h>
}

#include <string>

namespace Av
{


void AvInit();

class AvError
{
	public:
		AvError() : _errnum(0) {}
		AvError(int ernum) : _errnum(ernum) {}

		void operator=(int errnum) { _errnum = errnum; }

		operator bool() { return _errnum < 0; }

		std::string to_str() const;

		friend std::ostream& operator<<(std::ostream& ost, const AvError&);

		bool eof() { return _errnum == AVERROR_EOF; } // TODO

	private:
		int _errnum;
};

} // namespace Av

#endif
