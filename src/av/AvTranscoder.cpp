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
#include <atomic>
#include <mutex>

#include <boost/tokenizer.hpp>

#include "logger/Logger.hpp"

#include "AvTranscoder.hpp"

namespace Av {

#define LMS_LOG_TRANSCODE(sev)	LMS_LOG(TRANSCODE, INFO) << "[" << _id << "] - "

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
static const std::vector<std::string> execNames =
{
	"avconv",
	"ffmpeg",
};

static std::mutex		transcoderMutex;
static boost::filesystem::path	avConvPath = boost::filesystem::path();
static std::atomic<size_t>	globalId = {0};

static std::string searchPath(std::string filename)
{
	std::string path;

	path = ::getenv("PATH");
	if (path.empty())
		throw std::runtime_error("Environment variable PATH not found");

	std::string result;
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep(":");
	tokenizer tok(path, sep);
	for (tokenizer::iterator it = tok.begin(); it != tok.end(); ++it)
	{
		boost::filesystem::path p = *it;
		p /= filename;
		if (!::access(p.c_str(), X_OK))
		{
			result = p.string();
			break;
		}
	}
	return result;
}

void
Transcoder::init()
{
	for (std::string execName : execNames)
	{
		boost::filesystem::path p = searchPath(execName);
		if (!p.empty())
		{
			avConvPath = p;
			break;
		}
	}

	if (!avConvPath.empty())
		LMS_LOG(TRANSCODE, INFO) << "Using transcoder " << avConvPath;
	else
		throw std::runtime_error("Cannot find any transcoder binary!");
}

Transcoder::Transcoder(boost::filesystem::path filePath, TranscodeParameters parameters)
: _filePath(filePath),
  _parameters(parameters),
  _isComplete(false),
  _id(globalId++)
{

}

bool
Transcoder::start()
{
	if (!boost::filesystem::exists(_filePath))
		return false;
	else if (!boost::filesystem::is_regular( _filePath) )
		return false;

	LMS_LOG_TRANSCODE(INFO) << "Transcoding file '" << _filePath << "'";

	std::vector<std::string> args;

	args.push_back(avConvPath.string());

	// Make sure we do not produce anything in the stderr output
	// in order not to block the whole forked process
	args.push_back("-loglevel");
	args.push_back("quiet");

	// input Offset
	if (_parameters.getOffset().total_seconds() > 0)
	{
		args.push_back("-ss");
		args.push_back(std::to_string(_parameters.getOffset().total_seconds()));
	}

	// Input file
	args.push_back("-i");
	args.push_back(_filePath.string());

	// Output bitrates
	args.push_back("-b:a");
	args.push_back(std::to_string(_parameters.getBitrate(Stream::Type::Audio)));
//	if (_parameters.getOutputFormat().getType() == Format::Video)
//		oss << " -b:v " << _parameters.getOutputBitrate(Stream::Video);

	// Stream mapping
	for (int streamId : _parameters.getSelectedStreamIds())
	{
		// 0 means the first input file
		args.push_back("-map");
		args.push_back("0:" + std::to_string(streamId));
	}

	// Codecs and formats
	switch( _parameters.getEncoding())
	{
		case Encoding::MP3:
			args.push_back("-f");
			args.push_back("mp3");
			break;

		case Encoding::OGA:
			args.push_back("-acodec");
			args.push_back("libvorbis");
			args.push_back("-f");
			args.push_back("ogg");
			break;

		case Encoding::OGV:
			args.push_back("-acodec");
			args.push_back("libvorbis");
			args.push_back("-ac");
			args.push_back("2");
			args.push_back("-ar");
			args.push_back("44100");
			args.push_back("-vcodec");
			args.push_back("libtheora");
			args.push_back("-threads");
			args.push_back("4");
			args.push_back("-f");
			args.push_back("ogg");
			break;
		case Encoding::WEBMA:
			args.push_back("-codec:a");
			args.push_back("libvorbis");
			args.push_back("-f");
			args.push_back("webm");
			break;

		case Encoding::WEBMV:
			args.push_back("-acodec");
			args.push_back("libvorbis");
			args.push_back("-ac");
			args.push_back("2");
			args.push_back("-ar");
			args.push_back("44100");
			args.push_back("-vcodec");
			args.push_back("libvpx");
			args.push_back("-threads");
			args.push_back("4");
			args.push_back("-f");
			args.push_back("webm");
			break;

		case Encoding::M4A:
			args.push_back("-acodec");
			args.push_back("aac");
			args.push_back("-f");
			args.push_back("mp4");
			args.push_back("-strict");
			args.push_back("experimental");
			break;

		case Encoding::M4V:
			args.push_back("-acodec");
			args.push_back("aac");
			args.push_back("-strict");
			args.push_back("experimental");
			args.push_back("-ac");
			args.push_back("2");
			args.push_back("-ar");
			args.push_back("-44100");
			args.push_back("-vcodec");
			args.push_back("libx264");
			args.push_back("-f");
			args.push_back("m4v");
			break;

		case Encoding::FLV:
			args.push_back("-acodec");
			args.push_back("libmp3lame");
			args.push_back("-ac");
			args.push_back("2");
			args.push_back("-ar");
			args.push_back("44100");
			args.push_back("-vcodec");
			args.push_back("libx264");
			args.push_back("-f");
			args.push_back("flv");
			break;

		case Encoding::FLA:
			args.push_back("-acodec");
			args.push_back("libmp3lame");
			args.push_back("-f");
			args.push_back("flv");
			break;
		default:
			return false;
	}

	args.push_back("pipe:1");

	LMS_LOG_TRANSCODE(INFO) << "Dumping args (" << args.size() << ")";
	for (std::string arg : args)
		LMS_LOG_TRANSCODE(DEBUG) << "Arg = '" << arg << "'";

	// make sure only one thread is executing this part of code
	{
		std::lock_guard<std::mutex> lock(transcoderMutex);

		_child = std::make_shared<redi::ipstream>();

		const redi::pstreams::pmode mode = redi::pstreams::pstdout; // | redi::pstreams::pstderr;
		_child->open(avConvPath.string(), args, mode);
		if (!_child->is_open())
		{
			LMS_LOG_TRANSCODE(DEBUG) << "Exec failed!";
			return false;
		}

		if (_child->out().eof())
		{
			LMS_LOG_TRANSCODE(DEBUG) << "Early end of file!";
			return false;
		}

		LMS_LOG_TRANSCODE(DEBUG) << "Stream opened!";
	}

	return true;
}

void
Transcoder::process(std::vector<unsigned char>& output, std::size_t maxSize)
{
	if (!_child || _isComplete)
		return;

	output.resize(maxSize);

	//Read on the output stream
	_child->out().read(reinterpret_cast<char*>(&output[0]), maxSize);
	output.resize(_child->out().gcount());

	LMS_LOG_TRANSCODE(DEBUG) << "Read " << output.size() << " bytes";

	if (_child->out().eof())
	{
		LMS_LOG_TRANSCODE(DEBUG) << "Stdout EOF!";
		_child->clear();

		_isComplete = true;
		_child.reset();
	}

	_total += output.size();

	LMS_LOG_TRANSCODE(DEBUG) << "nb bytes = " << output.size() << ", total = " << _total;
}

Transcoder::~Transcoder()
{
	LMS_LOG_TRANSCODE(DEBUG) << ", ~Transcoder called! Total produced bytes = " << _total;

	if (_child)
	{
		LMS_LOG_TRANSCODE(DEBUG) << "Child still here!";
		_child->rdbuf()->kill(SIGKILL);
		LMS_LOG_TRANSCODE(DEBUG) << "Closing...";
		_child->rdbuf()->close();
		LMS_LOG_TRANSCODE(DEBUG) << "Closing DONE";
	}
}

} // namespace Transcode
