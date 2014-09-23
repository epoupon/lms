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

#include <cassert>
#include <stdexcept>
#include <iostream>

#include "logger/Logger.hpp"

#include "Codec.hpp"

Codec::Codec(enum CodecID codec, Type type )
: _codec(nullptr)
{
	if (type == Encoder)
		_codec = avcodec_find_encoder(codec);
	else if (type == Decoder)
		_codec = avcodec_find_decoder(codec);

	if (_codec == nullptr) {
		LMS_LOG(MOD_AV, SEV_ERROR) << "Codec constructor failed! codec = " << codec << ", type = " << type;
		throw std::runtime_error("can't find codec using this id!");
	}
}

Codec::Codec(const AVCodec* codec)
: _codec(codec)
{
	assert(_codec != nullptr);
}

Codec::~Codec()
{
	if (_codec == nullptr) {
		// TODO release iif ownership has not been taken?
//		av_codec_close(_codec);
	}
}

const AVCodec*
Codec::get() const
{
	assert(_codec != nullptr);
	return _codec;
}

Codec::Id
Codec::getId() const
{
	assert(_codec != nullptr);
	return _codec->id;
}

std::string
Codec::getName() const
{
	assert(_codec != nullptr);
	return _codec->name;
}

