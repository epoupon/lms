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

#include "AvTranscoder.hpp"

#include <atomic>
#include <thread>
#include <mutex>

#include "AvInfo.hpp"
#include "utils/Config.hpp"
#include "utils/IChildProcessManager.hpp"
#include "utils/Path.hpp"
#include "utils/Logger.hpp"
#include "utils/Service.hpp"

namespace Av {

#define LMS_LOG_TRANSCODE(sev)	LMS_LOG(TRANSCODE, sev) << "[" << _id << "] - "

static std::atomic<size_t>	globalId {};
static std::filesystem::path	ffmpegPath;

void
Transcoder::init()
{
	ffmpegPath = ServiceProvider<Config>::get()->getPath("ffmpeg-file", "/usr/bin/ffmpeg");
	if (!std::filesystem::exists(ffmpegPath))
		throw LmsException {"File '" + ffmpegPath.string() + "' does not exist!"};
}

Transcoder::Transcoder(const std::filesystem::path& filePath, const TranscodeParameters& parameters)
: _id {globalId++},
_filePath {filePath},
_parameters {parameters}
{
}

bool
Transcoder::start()
{
	if (!std::filesystem::exists(_filePath))
		return false;
	else if (!std::filesystem::is_regular_file( _filePath) )
		return false;

	LMS_LOG_TRANSCODE(INFO) << "Transcoding file '" << _filePath.string() << "'";

	std::vector<std::string> args;

	args.emplace_back(ffmpegPath.string());

	// Make sure we do not produce anything in the stderr output
	// and we not rely on input in order not to block the whole child process
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

	// Codecs and formats
	if (_parameters.encoding)
	{
		// Output bitrates
		if (_parameters.bitrate)
		{
			args.emplace_back("-b:a");
			args.emplace_back(std::to_string(*_parameters.bitrate));
		}

		switch (*_parameters.encoding)
		{
			case Encoding::MATROSKA_OPUS:
				assert(_parameters.bitrate);
				args.emplace_back("-acodec");
				args.emplace_back("libopus");
				args.emplace_back("-f");
				args.emplace_back("matroska");
				break;

			case Encoding::MP3:
				assert(_parameters.bitrate);
				args.emplace_back("-f");
				args.emplace_back("mp3");
				break;
			
			case Encoding::PCM_SIGNED_16_LE:
				args.emplace_back("-f");
				args.emplace_back("s16le");
				break;

			case Encoding::OGG_OPUS:
				assert(_parameters.bitrate);
				args.emplace_back("-acodec");
				args.emplace_back("libopus");
				args.emplace_back("-f");
				args.emplace_back("ogg");
				break;

			case Encoding::OGG_VORBIS:
				assert(_parameters.bitrate);
				args.emplace_back("-acodec");
				args.emplace_back("libvorbis");
				args.emplace_back("-f");
				args.emplace_back("ogg");
				break;

			case Encoding::WEBM_VORBIS:
				assert(_parameters.bitrate);
				args.emplace_back("-acodec");
				args.emplace_back("libvorbis");
				args.emplace_back("-f");
				args.emplace_back("webm");
				break;

			default:
				return false;
		}

		_outputMimeType = encodingToMimetype(*_parameters.encoding);
	}
	else
	{
		auto mediaFileFormat {guessMediaFileFormat(_filePath)};

		if (!mediaFileFormat)
		{
			LMS_LOG(AV, ERROR) << "Cannot guess media file format for '" << _filePath.string() << "'";
			return false;
		}

		args.emplace_back("-acodec");
		args.emplace_back("copy");
		args.emplace_back("-f");
		args.emplace_back(mediaFileFormat->format);

		_outputMimeType = mediaFileFormat->mimeType;
	}

	args.emplace_back("pipe:1");

	LMS_LOG_TRANSCODE(DEBUG) << "Dumping args (" << args.size() << ")";
	for (const std::string& arg : args)
		LMS_LOG_TRANSCODE(DEBUG) << "Arg = '" << arg << "'";

	try
	{
		_child = ServiceProvider<IChildProcessManager>::get()->spawnChildProcess(ffmpegPath, args);

		LMS_LOG_TRANSCODE(DEBUG) << "Stream opened!";
		return true;
	}
	catch (LmsException& e)
	{
		LMS_LOG_TRANSCODE(ERROR) << "Unable to create transcoder: " << e.what();
		return false;
	}
}

void
Transcoder::asyncWaitForData(WaitCallback cb)
{
	assert(_child);

	LMS_LOG_TRANSCODE(DEBUG) << "Want to wait for data";

	_child->asyncWaitForData([cb = std::move(cb)]()
	{
		cb();
	});
}

std::size_t
Transcoder::readSome(unsigned char* buffer, std::size_t bufferSize)
{
	assert(_child);

	return _child->readSome(buffer, bufferSize);
}

bool
Transcoder::finished() const
{
	return _child->finished();
}

} // namespace Transcode
