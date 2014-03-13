
#include <iostream>
#include <sstream>

#include <boost/iostreams/stream.hpp>
#include <boost/process.hpp>
#include <boost/foreach.hpp>

#include "AvConvTranscoder.hpp"

namespace Transcode
{

boost::filesystem::path	AvConvTranscoder::_avConvPath = "";

void
AvConvTranscoder::init()
{
	_avConvPath = boost::process::search_path("avconv");
	std::cout << "Using execPath " << _avConvPath << std::endl;
}


//boost::filesystem::path AvConvTranscoder::_avConvPath = "";

AvConvTranscoder::AvConvTranscoder(const Parameters& parameters)
: _parameters(parameters),
  _outputPipe(boost::process::create_pipe()),
  _source(_outputPipe.source, boost::iostreams::close_handle),
  _is(_source),
  _in(&_is),
  _isComplete(false)
{

	if (!boost::filesystem::exists(_parameters.getInputMediaFile().getPath())) {
		throw std::runtime_error("File " +  _parameters.getInputMediaFile().getPath().string() + " does not exists!");
	}
	else if (!boost::filesystem::is_regular( _parameters.getInputMediaFile().getPath())) {
		throw std::runtime_error("File " +  _parameters.getInputMediaFile().getPath().string() + " is not regular!");
	}

	std::cout << "Transcoding file '" << _parameters.getInputMediaFile().getPath() << "'" << std::endl;

	// Launch a process to handle the conversion
	boost::iostreams::file_descriptor_sink sink(_outputPipe.sink, boost::iostreams::close_handle);

	std::ostringstream oss;

	oss << "avconv";

	// input Offset
	if (_parameters.getOffset().total_seconds() > 0)
		oss << " -ss " << _parameters.getOffset().total_seconds();		// to be placed before '-i' to speed up seeking

	// Input file
	oss << " -i \"" << _parameters.getInputMediaFile().getPath().string() << "\"";

	// Output bitrates
	oss << " -b:a " << _parameters.getOutputAudioBitrate() ;
	if (_parameters.getOutputFormat().getType() == Format::Video)
		oss << " -b:v " << _parameters.getOutputVideoBitrate();

	// Stream mapping
	{
		typedef std::map<Stream::Type, Stream::Id> StreamMap;

		const StreamMap& streamMap = _parameters.getInputStreams();

		BOOST_FOREACH(const StreamMap::value_type& inputStream, streamMap)
		{
			// HACK (no subtitle support yet)
			if (inputStream.first != Stream::Subtitle)
				oss << " -map 0:" << inputStream.second;	// 0 means input file index
		}
	}

	// Codecs and formats
	switch( _parameters.getOutputFormat().getEncoding())
	{
		case Format::MP3:
			oss << " -f mp3";
			break;
		case Format::OGA:
			oss << " -acodec libvorbis -f ogg";
			break;
		case Format::OGV:
			oss << " -acodec libvorbis -ac 2 -ar 44100 -vcodec libtheora -threads 4 -f ogg";
			break;
		case Format::WEBMA:
			oss << " -codec:a libvorbis -f webm";
			break;
		case Format::WEBMV:
			oss << " -acodec libvorbis -ac 2 -ar 44100 -vcodec libvpx -threads 4 -f webm";
			break;
		case Format::M4A:
			oss << " -acodec aac -f mp4";
			break;
		case Format::M4V:
			oss << " -acodec aac -strict experimental -ac 2 -ar 44100 -vcodec libx264 -f m4v";
			break;
		case Format::FLV:
			oss << " -acodec libmp3lame -ac 2 -ar 44100 -vcodec libx264 -f flv";
			break;
		default:
			assert(0);
	}
	oss << " -";		// output to stdout


	std::cout << "executing... '" << oss.str() << "'" << std::endl;

	std::vector<int> ranges = { 3, 1024 };	// fd range to be closed
	_child = std::make_shared<boost::process::child>( boost::process::execute(
				boost::process::initializers::run_exe(_avConvPath),
				boost::process::initializers::set_cmd_line(oss.str()),
				boost::process::initializers::bind_stdout(sink),
				boost::process::initializers::close_fd(STDIN_FILENO),
				boost::process::initializers::close_fds(ranges)
				)
			);

}

	void
AvConvTranscoder::process(void)
{
	std::size_t readDatasSize = 1024; // TODO parametrize elsewhere?

	char ch;
	while(readDatasSize != 0 && _in && _in.get(ch)) {
		_data.push_back(ch);
		--readDatasSize;
	}

	if (!_in || _in.fail() || _in.eof()) {
		std::cout << "Transcode complete!" << std::endl;

		waitChild();
		_isComplete = true;
	}
}


AvConvTranscoder::~AvConvTranscoder()
{
	std::cout << "AvConvTranscoder::~AvConvTranscoder called!" << std::endl;

	if (_in.eof())
		waitChild();
	else
		killChild();
}

void
AvConvTranscoder::waitChild()
{
	try {
		if (_child) {
			std::cout << "waiting for child!" << std::endl;
			boost::process::wait_for_exit(*_child);
			std::cout << "waiting for child! DONE" << std::endl;
			_child.reset();
		}
	}
	catch( std::exception& e)
	{
		std::cerr << "Exception caugh in waitChild: " << e.what() << std::endl;
	}
}

	void
AvConvTranscoder::killChild()
{
	try {
		if (_child) {
			std::cout << "Killing child!" << std::endl;
			boost::process::terminate(*_child);
			std::cout << "Killing child DONE" << std::endl;
			_child.reset();
		}
	}
	catch( std::exception& e)
	{
		std::cerr << "Exception caugh in killChild: " << e.what() << std::endl;
	}
}

} // namespace Transcode
