#include <cassert>
#include "Stream.hpp"

namespace Av
{

Stream::Stream(AVStream* stream)
: _stream(stream)
{
	assert(_stream != nullptr);
	assert(_stream->codec != nullptr);
}

CodecContext
Stream::getCodecContext()
{
	return _stream->codec;
}

Dictionary
Stream::getMetadata(void)
{
	return Dictionary(_stream->metadata);
}

} // namespace Av
