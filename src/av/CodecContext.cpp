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

#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <boost/array.hpp>

#include "CodecContext.hpp"

namespace Av
{

CodecContext::CodecContext(AVCodecContext* CodecContext)
: _codecContext(CodecContext)
{
	assert(_codecContext != nullptr);
}


void
CodecContext::dumpInfo(std::ostream& ost) const
{
	ost << "BitRate = " << getBitRate() << ", SampleFormat = " << getSampleFormat() << ", SampleRate = " << getSampleRate() << ", ChannelLayout = " << getChannelLayout() << ", NbChannels = " << getNbChannels() << ", Timebase = " << getTimeBase().num << "/" << getTimeBase().den;
}


std::string
CodecContext::getCodecDesc(void) const
{
	std::array<char, 256> buf;
	avcodec_string(buf.data(), buf.size(), _codecContext, 0);

	return std::string(buf.data());
}

} // namespace Av

