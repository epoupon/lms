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

		Parameters(const InputMediaFile& InputFile, const Format& outputFormat, std::size_t audioBitrate);
		Parameters(const InputMediaFile& InputFile, const Format& outputFormat, std::size_t audioBitrate, std::size_t videoBitrate);

		// Modifiers
		void setOffset(boost::posix_time::time_duration offset)	{ _offset = offset; }	// Set input offset
		void setOutputFormat(const Format& format)		{ _outputFormat = format; }

		// Manually select an input stream to output
		// There can be only one stream per each type (video, audio, subtitle)
		typedef std::map<Stream::Type, Stream::Id> StreamMap;
		void 					selectInputStream(Stream::Type type, Stream::Id id)	{ _inputStreams[type] = id; }
		const StreamMap& 			getInputStreams(void) const	{return _inputStreams;}

		//Accessors
		boost::posix_time::time_duration	getOffset(void) const {return _offset;}
		const Format&				getOutputFormat(void) const { return _outputFormat; }
		std::size_t				getOutputAudioBitrate(void) const { return _outputAudioBitrate; }
		std::size_t				getOutputVideoBitrate(void) const { return _outputVideoBitrate; }
		const InputMediaFile&			getInputMediaFile(void) const { return _mediaFile;}
		InputMediaFile&				getInputMediaFile(void) { return _mediaFile;}


	private:

		InputMediaFile				_mediaFile;

		boost::posix_time::time_duration	_offset;		// start input offset

		Format					_outputFormat;		// OGA, OGV, etc.
		std::size_t				_outputAudioBitrate;	// 192000, 128000, etc.
		std::size_t				_outputVideoBitrate;	// 300000, 700000, etc.

		StreamMap				_inputStreams;

};

} // namespace Transcode

#endif

