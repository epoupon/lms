
#include <list>

#include <boost/foreach.hpp>

#include "InputMediaFile.hpp"

#include "av/InputFormatContext.hpp"
#include "cover/CoverArtGrabber.hpp"

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
	std::vector<Av::Stream> avStreams = input.getStreams();

	std::list<enum AVMediaType> avMediaTypes;	// List of encountered streams
	for (std::size_t avStreamId = 0; avStreamId < avStreams.size(); ++avStreamId)
	{
		Av::Stream&	avStream(avStreams[avStreamId]);
		Stream::Type	type;

		if (getStreamType(avStream.getCodecContext().getType(), type))
		{
			// Reject Video stream hat are in fact cover arts
			if (avStream.hasAttachedPic())
			{
				std::cout << "Rejecting stream since it is an attached picture!" << std::endl;
				continue;
			}


			avMediaTypes.push_back(avStream.getCodecContext().getType());

			_streams.push_back( Stream(avStreamId,
						type,
						avStream.getMetadata().get("language"),	// TODO define somewhere else?
						avStream.getCodecContext().getCodecDesc()
						));
		}
	}

	avMediaTypes.unique();
	// Scan for best streams
	BOOST_FOREACH(enum AVMediaType type, avMediaTypes)
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

	_covers = CoverArt::Grabber::getFromInputFormatContext(input);
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

