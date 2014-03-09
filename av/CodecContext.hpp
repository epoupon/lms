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

