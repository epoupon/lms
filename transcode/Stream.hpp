#ifndef TRANSCODE_STREAM_HPP
#define TRANSCODE_STREAM_HPP

#include <string>

namespace Transcode
{

class Stream
{

	public:

		enum Type
		{
			Audio,
			Video,
			Subtitle,
		};

		typedef std::size_t  Id;

		Stream(Id id, Type type, const std::string& lang, const std::string& desc)
			: _id(id), _type(type), _language(lang), _desc(desc) {}

		// Accessors
		Id			getId() const		{ return _id;}
		Type			getType() const		{ return _type;}
		const std::string&	getLanguage() const	{ return _language;}
		const std::string&	getDesc() const		{ return _desc;}

	private:

		Id		_id;
		Type		_type;
		std::string	_language;
		std::string	_desc;

};


} // namespace Transcode

#endif

