#include <stdexcept>
#include <iostream>

#include "InputFormatContext.hpp"

namespace Av
{

InputFormatContext::InputFormatContext(const boost::filesystem::path& p)
: _path(p)
{
	AVFormatContext* context = nullptr;

	// The last three parameters specify the file format, buffer size and
	// format parameters. By simply specifying NULL or 0 we ask libavformat
	// to auto-detect the format and use a default buffer size.
	AvError error = avformat_open_input(&context, p.string().c_str(), nullptr, nullptr);
	if (error)
	{
		std::cerr << "Cannot open '" << p.string() << "', avformat_open_input returned " << error << std::endl;
		throw std::runtime_error("avformat_open_input failed: " + error.to_str());
	}

	native(context);

}

InputFormatContext::~InputFormatContext()
{
	AVFormatContext* context = native();
	avformat_close_input(&context);
}

std::vector<Stream>
InputFormatContext::getStreams(void)
{
	std::vector<Stream> res;

	for (std::size_t i = 0; i < native()->nb_streams; ++i) {
		res.push_back( Stream(native()->streams[i]));
	}

	return res;
}


bool
InputFormatContext::getBestStreamIdx(AVMediaType type, Stream::Idx& index)
{
	int res = av_find_best_stream(native(),
			type,
			-1,	// Auto
			-1,	// Auto
			NULL,
			0
			);

	AvError error(res);
	if (error) {
		std::cerr << "Cannot get best stream for type " << type << ": " << error << std::endl;
		return false;
	}
	else {
		index = res;
		return true;
	}
}

void
InputFormatContext::findStreamInfo(void)
{
	native()->max_analyze_duration = 10 * AV_TIME_BASE; // 10 secs
	AvError err = avformat_find_stream_info(native(), NULL);
	if (err) {
		std::cerr << "Couldn't find stream information: " << err << std::endl;
		throw std::runtime_error("av_find_stream_info failed!");
	}
}


std::size_t
InputFormatContext::getDurationSecs() const
{
	if (native()->duration != AV_NOPTS_VALUE )
		return native()->duration / AV_TIME_BASE;
	else
		return 0;	// TODO, do something better?
}

Dictionary
InputFormatContext::getMetadata(void)
{
	return Dictionary(native()->metadata);

}

void
InputFormatContext::getPictures(std::vector< std::vector<unsigned char> >& pictures) const
{
	for (std::size_t i = 0; i < native()->nb_streams; ++i)
	{
		if (native()->streams[i]->disposition & AV_DISPOSITION_ATTACHED_PIC)
		{
			AVPacket pkt = native()->streams[i]->attached_pic;

			std::vector<unsigned char> data;
			std::copy(pkt.data, pkt.data + pkt.size, std::back_inserter(data));

			pictures.push_back( data );
		}
	}
}

} //namespace Av
