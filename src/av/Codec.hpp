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

#ifndef CODEC_HPP__
#define CODEC_HPP__

#include <string>
#include <memory>
#include <boost/utility.hpp>

#include "Common.hpp"

class Codec : boost::noncopyable
{
	friend class CodecContext;
	friend class FormatContext;
	friend class OutputFormatContext;

	public:

		typedef enum AVCodecID Id;
		enum Type {
			Encoder,
			Decoder,
		};

		Codec(Id codecId, Type type);
		Codec(const AVCodec* codec);		// Attach existing codec
		~Codec();

		Id		getId() const;
		std::string 	getName() const;

	private:

		const AVCodec* get() const;
		const AVCodec* _codec;
};

#endif
