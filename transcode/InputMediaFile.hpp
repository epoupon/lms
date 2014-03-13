#ifndef TRANSCODE_INPUT_MEDIA_FILE
#define TRANSCODE_INPUT_MEDIA_FILE

#include <map>
#include <string>

#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "Stream.hpp"


namespace Transcode
{



class InputMediaFile
{
	public:

		enum StreamType
		{
			StreamAudio,
			StreamVideo,
			StreamSubtitle,
		};

		InputMediaFile(const boost::filesystem::path& p);

		// Accessors
		boost::filesystem::path			getPath(void) const		{return _path;}
		boost::posix_time::time_duration	getDuration(void) const		{return _duration;}

		// Pictures
		const std::vector< std::vector<unsigned char> >&	getCoverPictures(void) const { return _coverPictures; }

		// Stream handling
		std::vector<Stream>			getStreams(Stream::Type type) const;
		const std::map<Stream::Type, Stream::Id>& getBestStreams(void) const	{return _bestStreams;}

	private:

		boost::filesystem::path			_path;
		boost::posix_time::time_duration	_duration;

		std::vector<Stream>			_streams;
		std::map<Stream::Type, Stream::Id>	_bestStreams;

		std::vector< std::vector<unsigned char> > _coverPictures;
};


} // namespace Transcode

#endif // TRANSCODE_INPUT_MEDIA_FILE

