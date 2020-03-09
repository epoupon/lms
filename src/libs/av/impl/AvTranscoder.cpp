/*

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

#include "av/AvTranscoder.hpp"

#include <atomic>
#include <mutex>

#include "av/AvInfo.hpp"
#include "utils/IConfig.hpp"
#include "utils/Path.hpp"
#include "utils/Logger.hpp"
#include "utils/Service.hpp"

namespace Av {

#define LOG(sev)	LMS_LOG(TRANSCODE, sev) << "[" << _id << "] - "

static std::atomic<size_t>	globalId {};
static std::filesystem::path	ffmpegPath;

void
Transcoder::init()
{
	ffmpegPath = ServiceProvider<IConfig>::get()->getPath("ffmpeg-file", "/usr/bin/ffmpeg");
	if (!std::filesystem::exists(ffmpegPath))
		throw LmsException {"File '" + ffmpegPath.string() + "' does not exist!"};
}

Transcoder::Transcoder(const std::filesystem::path& filePath, const TranscodeParameters& parameters)
: _filePath {filePath},
  _parameters {parameters},
  _id {globalId++}
{

}

bool
Transcoder::start()
{
	if (!std::filesystem::exists(_filePath))
		return false;
	else if (!std::filesystem::is_regular_file( _filePath) )
		return false;

	LOG(INFO) << "Transcoding file '" << _filePath.string() << "'";

	std::vector<std::string> args;

	args.emplace_back(ffmpegPath.string());

	// Make sure we do not produce anything in the stderr output
	// in order not to block the whole forked process
	args.emplace_back("-loglevel");
	args.emplace_back("quiet");
	args.emplace_back("-nostdin");

	// input Offset
	if (_parameters.offset)
	{
		args.emplace_back("-ss");
		args.emplace_back(std::to_string((*_parameters.offset).count()));
	}

	// Input file
	args.emplace_back("-i");
	args.emplace_back(_filePath.string());

	// Stream mapping, if set
	if (_parameters.stream)
	{
		args.emplace_back("-map");
		args.emplace_back("0:" + std::to_string(*_parameters.stream));
	}

	if (_parameters.stripMetadata)
	{
		// Strip metadata
		args.emplace_back("-map_metadata");
		args.emplace_back("-1");
	}

	// Skip video flows (including covers)
	args.emplace_back("-vn");

	// Output bitrates
	args.emplace_back("-b:a");
	args.emplace_back(std::to_string(_parameters.bitrate));

	// Codecs and formats
	switch (_parameters.encoding)
	{
		case Encoding::MP3:
			args.emplace_back("-f");
			args.emplace_back("mp3");
			break;

		case Encoding::OGG_OPUS:
			args.emplace_back("-acodec");
			args.emplace_back("libopus");
			args.emplace_back("-f");
			args.emplace_back("ogg");
			break;

		case Encoding::MATROSKA_OPUS:
			args.emplace_back("-acodec");
			args.emplace_back("libopus");
			args.emplace_back("-f");
			args.emplace_back("matroska");
			break;

		case Encoding::OGG_VORBIS:
			args.emplace_back("-acodec");
			args.emplace_back("libvorbis");
			args.emplace_back("-f");
			args.emplace_back("ogg");
			break;

		case Encoding::WEBM_VORBIS:
			args.emplace_back("-acodec");
			args.emplace_back("libvorbis");
			args.emplace_back("-f");
			args.emplace_back("webm");
			break;

		default:
			return false;
	}

	_outputMimeType = encodingToMimetype(_parameters.encoding);

	args.emplace_back("pipe:1");

	LOG(DEBUG) << "Dumping args (" << args.size() << ")";
	for (const std::string& arg : args)
		LOG(DEBUG) << "Arg = '" << arg << "'";

	// make sure only one thread is executing this part of code
	{
		static std::mutex transcoderMutex;

		std::lock_guard<std::mutex> lock {transcoderMutex};

		_child = std::make_shared<redi::ipstream>();

		// Caution: stdin must have been closed before
		_child->open(ffmpegPath.string(), args);
		if (!_child->is_open())
		{
			LOG(DEBUG) << "Exec failed!";
			return false;
		}

		if (_child->out().eof())
		{
			LOG(DEBUG) << "Early end of file!";
			return false;
		}

	}
	LOG(DEBUG) << "Stream opened!";

	return true;
}

void
Transcoder::process(std::vector<unsigned char>& output, std::size_t maxSize)
{
	if (!_child || _isComplete)
		return;

	if (_child->out().fail())
	{
		LOG(DEBUG) << "Stdout FAILED 2";
	}

	if (_child->out().eof())
	{
		LOG(DEBUG) << "Stdout ENDED 2";
	}

	output.resize(maxSize);

	//Read on the output stream
	_child->out().read(reinterpret_cast<char*>(&output[0]), maxSize);
	output.resize(_child->out().gcount());

	if (_child->out().fail())
	{
		LOG(DEBUG) << "Stdout FAILED";
	}

	if (_child->out().eof())
	{
		LOG(DEBUG) << "Stdout EOF!";
		_child->clear();

		_isComplete = true;
		_child.reset();
	}

	_total += output.size();

	LOG(DEBUG) << "nb bytes = " << output.size() << ", total = " << _total;
}

Transcoder::~Transcoder()
{
	LOG(DEBUG) << ", ~Transcoder called! Total produced bytes = " << _total;

	if (_child)
	{
		LOG(DEBUG) << "Child still here!";
		_child->rdbuf()->kill(SIGKILL);
		LOG(DEBUG) << "Closing...";
		_child->rdbuf()->close();
		LOG(DEBUG) << "Closing DONE";
	}
}

} // namespace Transcode
