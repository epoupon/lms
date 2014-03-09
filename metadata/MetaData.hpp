#ifndef METADATA_HPP
#define METADATA_HPP

#include <map>

#include <boost/any.hpp>
#include <boost/filesystem.hpp>

namespace MetaData
{

	enum Type
	{
		Artist,
		Title,
		Album,
		Genre,
		Duration,
		TrackNumber,
		DiscNumber,
		CreationTime,
		Cover,
	};

	// Used for Cover
	struct GenericData {
		std::string mimeType;
		std::vector<unsigned char> data;
	};

	typedef std::map<Type, boost::any>  Items;

	class Parser
	{
		public:

			typedef std::shared_ptr<Parser> pointer;

			virtual void parse(const boost::filesystem::path& p, Items& items) = 0;

	};

} // namespace MetaData

#endif

