#ifndef METADATA_HPP
#define METADATA_HPP

#include <map>

#include <boost/any.hpp>
#include <boost/filesystem.hpp>

namespace MetaData
{

	enum Type
	{
		Artist,			// string
		Title,			// string
		Album,			// string
		Genres,			// list<string>
		Duration,		// boost::posix_time::time_duration
		TrackNumber,		// size_t
		DiscNumber,		// size_t
		CreationTime,		// boost::posix_time::ptime
		Cover,			// GenericData
		AudioStreams,		// vector<AudioStream>
		VideoStreams,		// vector<VideoStream>
		SubtitleStreams,	// vector<SubtitleStream>
	};

	// Used by Cover
	struct GenericData {
		std::string mimeType;
		std::vector<unsigned char> data;
	};

	// Used by Streams
	struct AudioStream {
		std::size_t nbChannels;
		std::size_t bitRate;
	};

	struct VideoStream {
		std::size_t bitRate;
	};

	struct SubtitleStream {
		;
	};

	// Type and associated data
	// See enum Type's comments
	typedef std::map<Type, boost::any>  Items;

	class Parser
	{
		public:

			typedef std::shared_ptr<Parser> pointer;

			virtual void parse(const boost::filesystem::path& p, Items& items) = 0;

	};

} // namespace MetaData

#endif

