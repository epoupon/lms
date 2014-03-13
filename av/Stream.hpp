#ifndef STREAM_HPP__
#define STREAM_HPP__

#include "Common.hpp"

#include "CodecContext.hpp"
#include "Dictionary.hpp"

namespace Av
{

class Stream
{
	friend class InputFormatContext;

	public:
		// Attach existing stream
		Stream(AVStream* stream);

		typedef size_t Idx;

//	 	Idx getIdx() const { return _stream->index; }

		bool		hasAttachedPic(void) const;
		Dictionary	getMetadata(void);
		CodecContext	getCodecContext(void);

	private:

		AVStream*	_stream;
};

} // namespace Av

#endif

