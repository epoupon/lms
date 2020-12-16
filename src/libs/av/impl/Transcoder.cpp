/*
 * Copyright (C) 2020 Emeric Poupon
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

#include "Transcoder.hpp"

#include <atomic>
#include <iomanip>

#include "utils/IChildProcessManager.hpp"
#include "utils/IConfig.hpp"
#include "utils/Path.hpp"
#include "utils/Logger.hpp"
#include "utils/Service.hpp"

namespace Av {

#define LOG(sev)	LMS_LOG(TRANSCODE, sev) << "[" << _id << "] - "

static std::atomic<size_t>		globalId {};
static std::filesystem::path	ffmpegPath;

void
Transcoder::init()
{
	ffmpegPath = Service<IConfig>::get()->getPath("ffmpeg-file", "/usr/bin/ffmpeg");
	if (!std::filesystem::exists(ffmpegPath))
		throw Exception {"File '" + ffmpegPath.string() + "' does not exist!"};
}

Transcoder::Transcoder(const std::filesystem::path& filePath, const TranscodeParameters& parameters)
:  _id {globalId++}
, _filePath {filePath}
, _parameters {parameters}
{
}

Transcoder::~Transcoder() = default;

bool
Transcoder::start()
{
	if (ffmpegPath.empty())
		init();

	try
	{
		if (!std::filesystem::exists(_filePath))
		{
			LOG(ERROR) << "File '" << _filePath << "' does not exist!";
			return false;
		}
		else if (!std::filesystem::is_regular_file( _filePath) )
		{
			LOG(ERROR) << "File '" << _filePath << "' is not regular!";
			return false;
		}
	}
	catch (const std::filesystem::filesystem_error& e)
	{
		LOG(ERROR) << "File error on '" << _filePath.string() << "': " << e.what();
		return false;
	}

	LOG(INFO) << "Transcoding file '" << _filePath.string() << "'";

	std::vector<std::string> args;

	args.emplace_back(ffmpegPath.string());

	// Make sure:
	// - we do not produce anything in the stderr output
	// - we do not rely on input
	// in order not to block the whole forked process
	args.emplace_back("-loglevel");
	args.emplace_back("quiet");
	args.emplace_back("-nostdin");

	// input Offset
	{
		args.emplace_back("-ss");

		std::ostringstream oss;
		oss << std::fixed << std::showpoint << std::setprecision(3) << (_parameters.offset.count() / float {1000});
		args.emplace_back(oss.str());
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
	switch (_parameters.format)
	{
		case Format::MP3:
			args.emplace_back("-f");
			args.emplace_back("mp3");
			break;

		case Format::OGG_OPUS:
			args.emplace_back("-acodec");
			args.emplace_back("libopus");
			args.emplace_back("-f");
			args.emplace_back("ogg");
			break;

		case Format::MATROSKA_OPUS:
			args.emplace_back("-acodec");
			args.emplace_back("libopus");
			args.emplace_back("-f");
			args.emplace_back("matroska");
			break;

		case Format::OGG_VORBIS:
			args.emplace_back("-acodec");
			args.emplace_back("libvorbis");
			args.emplace_back("-f");
			args.emplace_back("ogg");
			break;

		case Format::WEBM_VORBIS:
			args.emplace_back("-acodec");
			args.emplace_back("libvorbis");
			args.emplace_back("-f");
			args.emplace_back("webm");
			break;

		default:
			return false;
	}

	_outputMimeType = formatToMimetype(_parameters.format);

	args.emplace_back("pipe:1");

	LOG(DEBUG) << "Dumping args (" << args.size() << ")";
	for (const std::string& arg : args)
		LOG(DEBUG) << "Arg = '" << arg << "'";

	// Caution: stdin must have been closed before
	try
	{
		_childProcess = Service<IChildProcessManager>::get()->spawnChildProcess(ffmpegPath, args);
	}
	catch (ChildProcessException& exception)
	{
		LOG(ERROR) << "Cannot execute '" << ffmpegPath << "': " << exception.what();
		return false;
	}

	return true;
}

void
Transcoder::asyncWaitForData(WaitCallback cb)
{
	assert(_childProcess);

	LOG(DEBUG) << "Want to wait for data";

	_childProcess->asyncWaitForData([cb = std::move(cb)]
	{
		cb();
	});
}

void
Transcoder::asyncRead(std::byte* buffer, std::size_t bufferSize, ReadCallback readCallback)
{
	assert(_childProcess);

	return _childProcess->asyncRead(buffer, bufferSize, [readCallback {std::move(readCallback)}](IChildProcess::ReadResult /*res*/, std::size_t nbBytesRead)
	{
		readCallback(nbBytesRead);
	});
}

std::size_t
Transcoder::readSome(std::byte* buffer, std::size_t bufferSize)
{
	assert(_childProcess);

	return _childProcess->readSome(buffer, bufferSize);
}

bool
Transcoder::finished() const
{
	return _childProcess->finished();
}

} // namespace Transcode
