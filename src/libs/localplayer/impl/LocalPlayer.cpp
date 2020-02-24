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

#include "LocalPlayer.hpp"

#include "database/Track.hpp"
#include "localplayer/IAudioOutput.hpp"
#include "utils/Logger.hpp"

std::unique_ptr<ILocalPlayer> createLocalPlayer(Database::Db& db)
{
	return std::make_unique<LocalPlayer>(db);
}

LocalPlayer::LocalPlayer(Database::Db& db)
: _dbSession {db}
{
	_ioService.setThreadCount(1);
	start();
}

LocalPlayer::~LocalPlayer()
{
	stop();
}

void
LocalPlayer::setAudioOutput(std::unique_ptr<IAudioOutput> audioOutput)
{
	_audioOutput = std::move(audioOutput);
	_audioOutput->setOnCanWriteCallback([=](std::size_t nbBytes) { handleNeedDataFromAudioOutput(nbBytes); });
}

const IAudioOutput*
LocalPlayer::getAudioOutput() const
{
	return _audioOutput.get();
}

void
LocalPlayer::start()
{
	LMS_LOG(LOCALPLAYER, INFO) << "Starting localplayer...";
	_ioService.start();
	LMS_LOG(LOCALPLAYER, INFO) << "Started localplayer!";
}

void
LocalPlayer::stop()
{
	LMS_LOG(LOCALPLAYER, INFO) << "Stopping localplayer...";
	_ioService.stop();
	LMS_LOG(LOCALPLAYER, INFO) << "Stopped localplayer!";
}

void
LocalPlayer::play()
{
	std::unique_lock lock {_mutex};

	std::filesystem::path trackPath;
	Av::TranscodeParameters parameters {};

	{
		auto transaction {_dbSession.createSharedTransaction()};

		auto track {Database::Track::getById(_dbSession, 49813)};
		if (!track)
		{
			LMS_LOG(LOCALPLAYER, DEBUG) << "Track not found";
			return; // TODO next song?
		}

		parameters.stripMetadata = true;
		parameters.encoding = Av::Encoding::PCM_SIGNED_16_LE;

		trackPath = track->getPath();
	}

	_transcoder = std::make_unique<Av::Transcoder>(trackPath, parameters);
	_transcoder->start();
	asyncWaitDataFromTranscoder();

}

void
LocalPlayer::pause()
{

}

void
LocalPlayer::addTrack(Database::IdType trackId)
{
	std::unique_lock lock {_mutex};

	_currentPlayqueue.push_back(trackId);
}

void
LocalPlayer::asyncWaitDataFromTranscoder()
{
	if (!_transcoder->finished())
	{
		_waitingDataFromTranscoder = true;
		_transcoder->asyncWaitForData([=]()
		{
			handleDataAvailableFromTranscoder();
		});
	}
}

void
LocalPlayer::handleDataAvailableFromTranscoder()
{
	std::unique_lock lock {_mutex};

	_waitingDataFromTranscoder = false;
	feedAudioOutputFromTranscoder(0);
}

std::size_t
LocalPlayer::feedAudioOutputFromTranscoder(std::size_t /*nbBytes*/)
{

	if (!_transcoder)
	{
		LMS_LOG(LOCALPLAYER, DEBUG) << "Transcoder not ready yet";
		return 0;
	}

	std::size_t audioOutputCanWriteBytesCount {_audioOutput->getCanWriteBytes()};

	LMS_LOG(LOCALPLAYER, DEBUG) << "Audio output needs " << audioOutputCanWriteBytesCount << " bytes";

	std::vector<unsigned char> buffer;
	buffer.resize(audioOutputCanWriteBytesCount);

	LMS_LOG(LOCALPLAYER, DEBUG) << "Reading up to " << buffer.size() << " bytes from transcoder";
	std::size_t nbTranscodedBytes {_transcoder->readSome(&buffer[0], buffer.size())};
	LMS_LOG(LOCALPLAYER, DEBUG) << "Got " << nbTranscodedBytes << " bytes from transcoder!";

	buffer.resize(nbTranscodedBytes);

	std::size_t nbWrittenBytes {_audioOutput->write(&buffer[0], buffer.size())};
	LMS_LOG(LOCALPLAYER, DEBUG) << "Written " << nbWrittenBytes << " bytes!";
	assert(nbWrittenBytes == nbTranscodedBytes);

	if (nbWrittenBytes < audioOutputCanWriteBytesCount)
	{
		LMS_LOG(LOCALPLAYER, DEBUG) << "not enough bytes from transcoder!";
		asyncWaitDataFromTranscoder();
	}

	return nbWrittenBytes;
}

void
LocalPlayer::handleNeedDataFromAudioOutput(std::size_t nbBytes)
{
	LMS_LOG(LOCALPLAYER, DEBUG) << nbBytes << " bytes needed from audio output!!";

	std::unique_lock lock {_mutex};
	if (!_waitingDataFromTranscoder)
		feedAudioOutputFromTranscoder(nbBytes);
	else
		LMS_LOG(LOCALPLAYER, DEBUG) << "Already waiting for data from transcoder";
}


