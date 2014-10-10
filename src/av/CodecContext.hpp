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

#ifndef CODEC_CONTEXT_HPP
#define CODEC_CONTEXT_HPP

#include <boost/utility.hpp>
#include <iostream>

#include "Codec.hpp"

namespace Av
{

class CodecContext
{
	public:

		CodecContext(AVCodecContext* CodecContext);	// Attach existing codec context (no free will be done)

//		Codec	getCodec();

		enum AVMediaType	getType(void) const	{ return _codecContext->codec_type; }
		Codec::Id		getCodecId(void) const	{ return _codecContext->codec_id; }

		std::string		getCodecDesc(void) const;

		// Accessors
		std::size_t	getBitRate() const			{return _codecContext->bit_rate;}
		AVSampleFormat	getSampleFormat() const			{return _codecContext->sample_fmt;}
		std::size_t	getSampleRate() const			{return _codecContext->sample_rate;}
		std::uint64_t	getChannelLayout() const		{return _codecContext->channel_layout;}
		std::size_t	getNbChannels() const			{return _codecContext->channels; }
		AVRational	getTimeBase() const 			{return _codecContext->time_base; }

		void dumpInfo(std::ostream& ost) const;

	private:
		AVCodecContext* native() { return _codecContext; }

		AVCodecContext* _codecContext;
};

} // namespace Av

#endif

