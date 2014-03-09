#include "av/InputFormatContext.hpp"

#include "Parameters.hpp"

namespace Transcode
{

std::string
getMimeType(Format format)
{
	//TODO
	return "";
}

Parameters::Parameters(const InputMediaFile& inputMediaFile,
		const Format& outputFormat,
		std::size_t audioBitrate)
	:
_mediaFile(inputMediaFile),
_outputFormat(outputFormat),
_outputAudioBitrate(audioBitrate),
_outputVideoBitrate(0)
{
	// By default, select the best stream indexes
	_inputStreams = _mediaFile.getBestStreams();
}

Parameters::Parameters(const InputMediaFile& inputMediaFile,
		const Format& outputFormat,
		std::size_t audioBitrate,
		std::size_t videoBitrate)
	:
_mediaFile(inputMediaFile),
_outputFormat(outputFormat),
_outputAudioBitrate(audioBitrate),
_outputVideoBitrate(videoBitrate)
{
	// By default, select the best stream indexes
	_inputStreams = _mediaFile.getBestStreams();
}

} // namespace Transcode

