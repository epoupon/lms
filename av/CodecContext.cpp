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
	std::vector<char> buf(256, 0);
	avcodec_string(&buf[0], buf.size(), _codecContext, 0);

	return std::string(buf.begin(), buf.end());
}

} // namespace Av

