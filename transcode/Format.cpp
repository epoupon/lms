#include <boost/foreach.hpp>
#include <stdexcept>
#include <cassert>

#include "Format.hpp"

namespace Transcode
{

const std::vector<Format> Format::_supportedFormats
{
	{Format::OGA, Format::Audio, "audio/ogg", "Ogg"},
	{Format::OGV, Format::Video, "video/ogg", "Ogg"},
	{Format::MP3, Format::Audio, "audio/mp3", "MP3"},
	{Format::WEBMA, Format::Audio, "audio/webm", "WebM"},
	{Format::WEBMV, Format::Video, "video/webm", "WebM"},
	{Format::FLV, Format::Video, "video/x-flv", "Flash Video"},
	{Format::M4A, Format::Audio, "audio/mp4", "MP4"},
	{Format::M4V, Format::Video, "video/mp4", "MP4"},
};

Format::Format(Encoding encoding, Type type, std::string mimeType, std::string desc)
:
_encoding(encoding),
_type(type),
_mineType(mimeType),
_desc(desc)
{}


const Format&
Format::get(Encoding encoding)
{
	BOOST_FOREACH(const Format& format, _supportedFormats)
	{
		if (format.getEncoding() == encoding)
			return format;
	}

	throw std::runtime_error("Cannot find format");
}

std::vector<Format>
Format::get(Type type)
{
	std::vector<Format> res;
	BOOST_FOREACH(const Format& format, _supportedFormats)
	{
		if (format.getType() == type)
			res.push_back( format );
	}

	return res;
}

bool
Format::operator==(const Format& other) const
{
	return getEncoding() == other.getEncoding();
}


} // namespace Transcode

