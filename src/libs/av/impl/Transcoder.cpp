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

#define LOG(sev)	LMS_LOG(TRANSCODE, sev) << "[" << _debugId << "] - "

static std::atomic<size_t>		globalId {};
static std::filesystem::path	ffmpegPath;

void
Transcoder::init()
{
	ffmpegPath = Service<IConfig>::get()->getPath("ffmpeg-file", "/usr/bin/ffmpeg");
	if (!std::filesystem::exists(ffmpegPath))
		throw Exception {"File '" + ffmpegPath.string() + "' does not exist!"};
}

Transcoder::Transcoder(const InputFileParameters& inputFileParameters, const TranscodeParameters& transcodeParameters)
:  _debugId {globalId++}
, _inputFileParameters {inputFileParameters}
, _transcodeParameters {transcodeParameters}
{
	start();
}

Transcoder::~Transcoder() = default;

void
Transcoder::start()
{
	if (ffmpegPath.empty())
		init();

	try
	{
		if (!std::filesystem::exists(_inputFileParameters.trackPath))
			throw Exception {"File '" + _inputFileParameters.trackPath.string() + "' does not exist!"};
		else if (!std::filesystem::is_regular_file( _inputFileParameters.trackPath) )
			throw Exception {"File '" + _inputFileParameters.trackPath.string() + "' is not regular!"};
	}
	catch (const std::filesystem::filesystem_error& e)
	{
		throw Exception {"File error '" + _inputFileParameters.trackPath.string() + "': " + e.what()};
	}

	LOG(INFO) << "Transcoding file '" << _inputFileParameters.trackPath.string() << "'";

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
		oss << std::fixed << std::showpoint << std::setprecision(3) << (_transcodeParameters.offset.count() / float {1000});
		args.emplace_back(oss.str());
	}

	// Input file
	args.emplace_back("-i");
	args.emplace_back(_inputFileParameters.trackPath.string());

	// Stream mapping, if set
	if (_transcodeParameters.stream)
	{
		args.emplace_back("-map");
		args.emplace_back("0:" + std::to_string(*_transcodeParameters.stream));
	}

	if (_transcodeParameters.stripMetadata)
	{
		// Strip metadata
		args.emplace_back("-map_metadata");
		args.emplace_back("-1");
	}

	// Skip video flows (including covers)
	args.emplace_back("-vn");

	// Output bitrates
	args.emplace_back("-b:a");
	args.emplace_back(std::to_string(_transcodeParameters.bitrate));

	// Codecs and formats
	switch (_transcodeParameters.format)
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
			throw Exception {"Unhandled format (" + std::to_string(static_cast<int>(_transcodeParameters.format)) + ")"};
	}

	_outputMimeType = formatToMimetype(_transcodeParameters.format);

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
		throw Exception {"Cannot execute '" + ffmpegPath.string() + "': " + exception.what()};
	}
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
	assert(_childProcess);

	return _childProcess->finished();
}

} // namespace Transcode
