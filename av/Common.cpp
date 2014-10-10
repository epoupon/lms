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

#include <boost/array.hpp>

#include "logger/Logger.hpp"

#include "Common.hpp"

namespace Av
{

std::string
AvError::to_str(void) const
{
	boost::array<char, 128> buf = {0};
	if (av_strerror(_errnum, buf.data(), buf.size()) == 0)
		return std::string(&buf[0]);
	else
		return "Unknown error";

}


std::ostream& operator<<(std::ostream& ost, const AvError& err)
{
	ost << err.to_str();
	return ost;
}

void AvInit()
{
	/* register all the codecs */
	avcodec_register_all();
	av_register_all();
	LMS_LOG(MOD_AV, SEV_INFO) << "avcodec version = " << avcodec_version();
}

} // namespace Av
