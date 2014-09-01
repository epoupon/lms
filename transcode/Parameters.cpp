
#include "logger/Logger.hpp"

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
		const Format& outputFormat)
	:
_mediaFile(inputMediaFile),
_outputFormat(outputFormat)
{
	// By default, select the best stream indexes
	_inputStreams = _mediaFile.getBestStreams();

//	setBitrate(Stream::Audio, 0);
//	setBitrate(Stream::Video, 0);
}

std::size_t
Parameters::setBitrate(Stream::Type type, std::size_t bitrate)
{
	// Limit the output bitrate to the input bitrate
	if (_inputStreams.find(type) != _inputStreams.end())
	{
		const Transcode::Stream& stream = _mediaFile.getStream( _inputStreams[type] );

		LMS_LOG(MOD_TRANSCODE, SEV_DEBUG) << "Stream bitrate = " << stream.getBitrate();

		if (!bitrate || bitrate > stream.getBitrate())
		{
			LMS_LOG(MOD_TRANSCODE, SEV_INFO) << "Setting bitrate for stream idx " << _inputStreams[type] << " to input bitrate (" << stream.getBitrate() << ")";
			_outputBitrate[type] = stream.getBitrate();
		}
		else
		{
			LMS_LOG(MOD_TRANSCODE, SEV_DEBUG) << "Setting bitrate for stream idx " << _inputStreams[type] << " to " << bitrate;
			_outputBitrate[type] = bitrate;
		}
		return _outputBitrate[type];
	}
	else
	{
		LMS_LOG(MOD_TRANSCODE, SEV_DEBUG) << "Cannot find stream type " << type;
		return 0;
	}
}

std::size_t
Parameters::getOutputBitrate(Stream::Type type) const
{
	std::map<Stream::Type, std::size_t>::const_iterator it = _outputBitrate.find(type);

	if (it != _inputStreams.end())
	{
		return it->second;
	}
	else
	{
		LMS_LOG(MOD_TRANSCODE, SEV_DEBUG) << "output bitrate not set for type " << type;
		return 0;
	}
}

} // namespace Transcode

