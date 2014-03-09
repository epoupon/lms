
#include <list>

#include <boost/foreach.hpp>

#include "InputMediaFile.hpp"

#include "av/InputFormatContext.hpp"

namespace Transcode
{

bool getStreamType(enum AVMediaType type, Transcode::Stream::Type& streamType)
{
	switch (type) {
		case AVMEDIA_TYPE_VIDEO:	streamType = Transcode::Stream::Video; return true;
		case AVMEDIA_TYPE_AUDIO:	streamType = Transcode::Stream::Audio; return true;
		case AVMEDIA_TYPE_SUBTITLE:	streamType = Transcode::Stream::Subtitle; return true;
		default:
			return false;
	}
}


InputMediaFile::InputMediaFile(const boost::filesystem::path& p)
: _path(p)
{
	Av::InputFormatContext input(_path);
	input.findStreamInfo();

	// Calculate estimated duration
	if (input.getDurationSecs())
		_duration	= boost::posix_time::seconds(input.getDurationSecs() + 1);

	// Get input streams
	std::vector<Av::Stream> streams = input.getStreams();

	std::list<enum AVMediaType> types;
	for (std::size_t streamId = 0; streamId < streams.size(); ++streamId)
	{
		Stream::Type type;

		if (getStreamType(streams[streamId].getCodecContext().getType(), type))
		{
			types.push_back(streams[streamId].getCodecContext().getType());

			_streams.push_back( Stream(streamId,
						type,
						streams[streamId].getMetadata().get("language"),	// TODO define somewhere else?
						streams[streamId].getCodecContext().getCodecDesc()
						));
		}
	}

	types.unique();
	// Scan for best streams
	BOOST_FOREACH(enum AVMediaType type, types)
	{
		Av::Stream::Idx index;

		if (input.getBestStreamIdx(type, index))
		{
			Stream::Type streamType;
			if (getStreamType(type, streamType))
				_bestStreams.insert(std::make_pair( streamType, index) );
		}
		else
			std::cerr << "Cannot find best stream for type " << type << std::endl;
	}

}

std::vector<Stream>
InputMediaFile::getStreams(Stream::Type type) const
{
	std::vector<Stream> res;
	BOOST_FOREACH(const Stream& stream, _streams)
	{
		if (stream.getType() == type)
			res.push_back(stream);
	}
	return res;
}


} // namespace Transcode

