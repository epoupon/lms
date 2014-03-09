#ifndef TRANSCODE_FORMAT_HPP
#define TRANSCODE_FORMAT_HPP

#include <vector>
#include <string>

namespace Transcode
{


class Format
{
	public:

		// Output format
		enum Encoding {
			OGA,
			OGV,
			MP3,
			WEBMA,
			WEBMV,
			FLV,
			M4A,
			M4V,
		};

		enum Type {
			Video,
			Audio,
		};

		// Utility
		static const Format&			get(Encoding encoding);
		static std::vector<Format>		get(Type type);

		Format(Encoding format, Type type, std::string mimeType, std::string desc);

		Encoding		getEncoding(void) const		{ return _encoding;}
		Type			getType(void) const		{ return _type;}
		const std::string&	getMimeType(void) const		{ return _mineType;}
		const std::string&	getDesc(void) const		{ return _desc;}

		bool	operator==(const Format& other) const;

	private:
		Encoding	_encoding;
		Type		_type;
		std::string	_mineType;
		std::string	_desc;

		static const std::vector<Format>	_supportedFormats;

};

} // namespace Transcode

#endif
