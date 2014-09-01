#ifndef TRANSCODE_PARAMETERS_HPP
#define TRANSCODE_PARAMETERS_HPP

#include <string>

#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "Format.hpp"

#include "InputMediaFile.hpp"

namespace Transcode
{


class Parameters {

	public:

		Parameters(const InputMediaFile& InputFile, const Format& outputFormat);

		// Modifiers
		void setOffset(boost::posix_time::time_duration offset)	{ _offset = offset; }	// Set input offset
		void setOutputFormat(const Format& format)		{ _outputFormat = format; }
		std::size_t setBitrate(Stream::Type type, std::size_t bitrate);

		// Manually select an input stream to output
		// There can be only one stream per each type (video, audio, subtitle)
		typedef std::map<Stream::Type, Stream::Id> StreamMap;
		void 					selectInputStream(Stream::Type type, Stream::Id id)	{ _inputStreams[type] = id; }
		const StreamMap& 			getInputStreams(void) const	{return _inputStreams;}

		//Accessors
		boost::posix_time::time_duration	getOffset(void) const {return _offset;}
		const Format&				getOutputFormat(void) const { return _outputFormat; }
		std::size_t				getOutputBitrate(Stream::Type type) const;
		const InputMediaFile&			getInputMediaFile(void) const { return _mediaFile;}
		InputMediaFile&				getInputMediaFile(void) { return _mediaFile;}


	private:

		InputMediaFile				_mediaFile;

		boost::posix_time::time_duration	_offset;		// start input offset

		Format					_outputFormat;		// OGA, OGV, etc.
		std::map<Stream::Type, std::size_t>	_outputBitrate;

		StreamMap				_inputStreams;

};

} // namespace Transcode

#endif

