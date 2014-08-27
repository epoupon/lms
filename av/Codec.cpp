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

