/*
 * Copyright (C) 2015 Emeric Poupon
 *
 * This file is part of LMS.
 *
 * LMS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LMS.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "logger/Logger.hpp"

#include "AvTranscoder.hpp"

namespace Av {

std::string encoding_to_mimetype(Encoding encoding)
{
	switch(encoding)
	{
		case Encoding::MP3: return "audio/mp3";
		case Encoding::OGA: return "audio/ogg";
		case Encoding::OGV: return "video/ogg";
		case Encoding::WEBMA: return "audio/webm";
		case Encoding::WEBMV: return "video/webm";
		case Encoding::FLA: return "audio/x-flv";
		case Encoding::FLV: return "video/x-flv";
		case Encoding::M4A: return "audio/mp4";
		case Encoding::M4V: return "video/mp4";
	}

	return "";
}

// TODO, parametrize?
const std::vector<std::string> execNames =
{
	"avconv",
	"ffmpeg",
};

boost::mutex	Transcoder::_mutex;

boost::filesystem::path	Transcoder::_avConvPath = boost::filesystem::path();

void
Transcoder::init()
{
	for (std::string execName : execNames)
	{
		const boost::filesystem::path p = boost::process::search_path(execName);
		if (!p.empty())
		{
			_avConvPath = p;
			break;
		}
	}

	if (!_avConvPath.empty())
		LMS_LOG(TRANSCODE, INFO) << "Using transcoder " << _avConvPath;
	else
		throw std::runtime_error("Cannot find any transcoder binary!");
}


//boost::filesystem::path Transcoder::_avConvPath = "";

Transcoder::Transcoder(boost::filesystem::path filePath, TranscodeParameters parameters)
: _filePath(filePath),
  _parameters(parameters),
  _outputPipe(boost::process::create_pipe()),
  _source(_outputPipe.source, boost::iostreams::close_handle),
  _is(_source),
  _in(&_is),
  _isComplete(false)
{

}

bool
Transcoder::start()
{
	if (!boost::filesystem::exists(_filePath))
		return false;
	else if (!boost::filesystem::is_regular( _filePath) )
		return false;

	LMS_LOG(TRANSCODE, INFO) << "Transcoding file '" << _filePath << "'";

	// Launch a process to handle the conversion
	boost::iostreams::file_descriptor_sink sink(_outputPipe.sink, boost::iostreams::close_handle);

	std::ostringstream oss;

	oss << _avConvPath;

	// input Offset
	if (_parameters.getOffset().total_seconds() > 0)
		oss << " -ss " << _parameters.getOffset().total_seconds();		// to be placed before '-i' to speed up seeking?

	// Input file
	oss << " -i " << _filePath;

	// Output bitrates
	oss << " -b:a " << _parameters.getBitrate(Stream::Type::Audio) ;
//	if (_parameters.getOutputFormat().getType() == Format::Video)
//		oss << " -b:v " << _parameters.getOutputBitrate(Stream::Video);

	// Stream mapping
	for (int streamId : _parameters.getSelectedStreamIds())
	{
		// 0 means the first input file
		oss << " -map 0:" << streamId;
	}

	// Codecs and formats
	switch( _parameters.getEncoding())
	{
		case Encoding::MP3:
			oss << " -f mp3";
			break;
		case Encoding::OGA:
			oss << " -acodec libvorbis -f ogg";
			break;
		case Encoding::OGV:
			oss << " -acodec libvorbis -ac 2 -ar 44100 -vcodec libtheora -threads 4 -f ogg";
			break;
		case Encoding::WEBMA:
			oss << " -codec:a libvorbis -f webm";
			break;
		case Encoding::WEBMV:
			oss << " -acodec libvorbis -ac 2 -ar 44100 -vcodec libvpx -threads 4 -f webm";
			break;
		case Encoding::M4A:
			oss << " -acodec aac -f mp4 -strict experimental";
			break;
		case Encoding::M4V:
			oss << " -acodec aac -strict experimental -ac 2 -ar 44100 -vcodec libx264 -f m4v";
			break;
		case Encoding::FLV:
			oss << " -acodec libmp3lame -ac 2 -ar 44100 -vcodec libx264 -f flv";
			break;
		case Encoding::FLA:
			oss << " -acodec libmp3lame -f flv";
			break;
		default:
			return false;
	}
	oss << " -";		// output to stdout

	LMS_LOG(TRANSCODE, DEBUG) << "Executing '" << oss.str() << "'";

	// make sure only one thread is executing this part of code
	// See boost process FAQ
	{
		boost::lock_guard<boost::mutex>	lock(_mutex);

		_child = std::make_shared<boost::process::child>( boost::process::execute(
					boost::process::initializers::run_exe(_avConvPath),
					boost::process::initializers::set_cmd_line(oss.str()),
					boost::process::initializers::bind_stdout(sink),
					boost::process::initializers::close_fds_if([](int fd) { return fd != STDOUT_FILENO;})
					)
				);
	}

	return true;
}

void
Transcoder::process(std::vector<unsigned char>& output, std::size_t maxSize)
{
	std::size_t readDataSize = 0;

	if (_isComplete)
		return;

	char ch;
	while(readDataSize < maxSize && _in && _in.get(ch)) {
		output.push_back(ch);
		readDataSize++;
	}

	if (!_in || _in.fail() || _in.eof()) {
		LMS_LOG(TRANSCODE, DEBUG) << "Transcode complete!";

		waitChild();
		_isComplete = true;
	}
}


Transcoder::~Transcoder()
{
	LMS_LOG(TRANSCODE, DEBUG) << "~Transcoder called!";

	if (_in.eof())
		waitChild();
	else
		killChild();
}

void
Transcoder::waitChild()
{
	if (_child)
	{
		boost::system::error_code ec;

		LMS_LOG(TRANSCODE, DEBUG) << "Waiting for child...";
		boost::process::wait_for_exit(*_child, ec);
		LMS_LOG(TRANSCODE, DEBUG) << "Waiting for child: OK";

		if (ec)
			LMS_LOG(TRANSCODE, ERROR) << "Transcoder::waitChild: error: " << ec.message();

		_child.reset();
	}
}

void
Transcoder::killChild()
{
	if (_child)
	{
		boost::system::error_code ec;

		LMS_LOG(TRANSCODE, DEBUG) << "Killing child! pid = " << _child->pid;
		boost::process::terminate(*_child, ec);
		LMS_LOG(TRANSCODE, DEBUG) << "Killing child DONE";

		// If an error occured, force kill the child
		if (ec)
			LMS_LOG(TRANSCODE, ERROR) << "Transcoder::killChild: error: " << ec.message();

		_child.reset();
	}
}

bool
Transcoder::isComplete(void)
{
	return _isComplete;
}

} // namespace Transcode
