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

#include <iomanip>

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

	LMS_LOG(LOCALPLAYER, INFO) << "Starting localplayer...";
	_ioService.start();
	LMS_LOG(LOCALPLAYER, INFO) << "Started localplayer!";
}

LocalPlayer::~LocalPlayer()
{
	LMS_LOG(LOCALPLAYER, INFO) << "Stopping localplayer...";
	_ioService.stop();
	LMS_LOG(LOCALPLAYER, INFO) << "Stopped localplayer!";
}

void
LocalPlayer::setAudioOutput(std::unique_ptr<IAudioOutput> audioOutput)
{
	_audioOutput = std::move(audioOutput);
	_audioOutput->setOnCanWriteCallback([this](std::size_t)
	{
		_ioService.post([this]()
		{
			std::unique_lock lock {_mutex};
			handleNeedDataFromAudioOutput();
		});
	});
}

const IAudioOutput*
LocalPlayer::getAudioOutput() const
{
	return _audioOutput.get();
}

void
LocalPlayer::play()
{
	handlePlay();
}

void
LocalPlayer::playEntry(EntryIndex id, std::chrono::milliseconds offset)
{
	std::unique_lock lock {_mutex};

	handlePlay(id, offset, true);
}

void
LocalPlayer::stop()
{
	std::unique_lock lock {_mutex};

	handleStop();
}

std::optional<Database::IdType>
LocalPlayer::getTrackIdFromPlayQueueIndex(EntryIndex entryId)
{
	if (entryId >= _currentPlayQueue.size())
	{
		LMS_LOG(LOCALPLAYER, DEBUG) << "Want to play an out of bound track";
		return std::nullopt;
	}

	return _currentPlayQueue[entryId];
}

void
LocalPlayer::handlePlay(std::optional<EntryIndex> id, std::chrono::milliseconds offset, bool immediate)
{
	if (_playState == Status::PlayState::Stopped)
		_audioOutput->start();

	startPlay(id, offset, immediate);
}

void
LocalPlayer::handleStop()
{
	_transcoder.reset();
	_audioOutput->stop();
	_audioOutputEntries.clear();
	_playState = Status::PlayState::Stopped;
	LMS_LOG(LOCALPLAYER, INFO) << "Player now stopped";
}

void
LocalPlayer::startPlay(std::optional<EntryIndex> id, std::chrono::milliseconds offset, bool immediate)
{
	assert(!_mutex.try_lock());

	if (id)
		_currentPlayQueueIdx = *id;

	if (!_currentPlayQueueIdx)
		_currentPlayQueueIdx = 0;

	while (_currentPlayQueueIdx < _currentPlayQueue.size())
	{
		if (startPlayQueueEntry(*_currentPlayQueueIdx, offset))
		{
			if (immediate)
				_nextWriteOffset = _audioOutput->getCurrentReadTime();

			_audioOutputEntries.emplace_back(
				AudioOutputEntryInfo
				{
					_nextWriteOffset ? *_nextWriteOffset : _audioOutput->getCurrentWriteTime(),
					offset,
					*_currentPlayQueueIdx
				});
			_playState = Status::PlayState::Playing;

			LMS_LOG(LOCALPLAYER, DEBUG) << "Adding new entry @ " << std::fixed << std::setprecision(3) << _audioOutputEntries.back().audioOutputStartTime.count() / float {1000};

			return;
		}

		(*_currentPlayQueueIdx)++;
		offset = {};
	}

	LMS_LOG(LOCALPLAYER, DEBUG) << "No more song in playqueue> stopping";
	_currentPlayQueueIdx.reset();
	handleStop();
}

bool
LocalPlayer::startPlayQueueEntry(std::size_t playqueueIdx, std::chrono::milliseconds offset)
{
	LMS_LOG(LOCALPLAYER, DEBUG) << "Playing playQueue entry " << playqueueIdx;

	const std::optional<Database::IdType> trackId {getTrackIdFromPlayQueueIndex(playqueueIdx)};
	if (!trackId)
		return false;

	std::filesystem::path trackPath;
	Av::TranscodeParameters parameters {};

	{
		auto transaction {_dbSession.createSharedTransaction()};

		auto track {Database::Track::getById(_dbSession, trackId.value())};
		if (!track)
		{
			LMS_LOG(LOCALPLAYER, DEBUG) << "Track not found";
			return false;
		}

		parameters.stripMetadata = true;
		parameters.encoding = Av::Encoding::PCM_SIGNED_16_LE; // TODO
		parameters.offset = offset;

		trackPath = track->getPath();
	}

	_transcoder = std::make_unique<Av::Transcoder>(trackPath, parameters);
	_transcoder->start();

	asyncWaitDataFromTranscoder();

	return true;
}

void
LocalPlayer::handleTranscoderFinished()
{
	LMS_LOG(LOCALPLAYER, DEBUG) << "Transcoder finished!";
	if (_playState == Status::PlayState::Playing)
	{
		if (_currentPlayQueueIdx)
			(*_currentPlayQueueIdx)++;

		startPlay();
	}
}

void
LocalPlayer::pause()
{

}

const LocalPlayer::AudioOutputEntryInfo*
LocalPlayer::getAudioOutputEntryInfo(std::chrono::milliseconds time) const
{
	for (auto it {std::crbegin(_audioOutputEntries)}; it != std::crend(_audioOutputEntries); ++it)
	{
		if (time >= it->audioOutputStartTime)
			return &(*it);
	}

	return {};
}

LocalPlayer::Status
LocalPlayer::getStatus()
{
	std::unique_lock lock {_mutex};

	LMS_LOG(LOCALPLAYER, DEBUG) << "Get status...";

	Status status;

	status.playState = _playState;

	if (status.playState != Status::PlayState::Stopped)
	{
		const auto currentReadTime {_audioOutput->getCurrentReadTime()};
		const AudioOutputEntryInfo* entryInfo {getAudioOutputEntryInfo(currentReadTime)};

		if (entryInfo)
		{
			const auto playedTime {currentReadTime - entryInfo->audioOutputStartTime};	
			LMS_LOG(LOCALPLAYER, DEBUG) << "track offset = " << entryInfo->trackOffset.count() << " usecs";
			status.currentPlayTime = entryInfo->trackOffset + playedTime;
			status.entryIdx = entryInfo->entryIndex;
		
			LMS_LOG(LOCALPLAYER, DEBUG) << "*** current time = " << std::fixed << std::setprecision(3) << status.currentPlayTime->count() / float {1000};
		}
	}

	return status;
}

void
LocalPlayer::clearTracks()
{
	std::unique_lock lock {_mutex};

	_currentPlayQueue.clear();
}

void
LocalPlayer::addTrack(Database::IdType trackId)
{
	std::unique_lock lock {_mutex};

	_currentPlayQueue.push_back(trackId);
}

void
LocalPlayer::asyncWaitDataFromTranscoder()
{
	if (!_transcoder->finished())
	{
		_waitingDataFromTranscoder = true;
		_transcoder->asyncWaitForData([=]()
		{
			_ioService.post([this]()
			{
				std::unique_lock lock {_mutex};
				handleDataAvailableFromTranscoder();
			});
		});
	}
	else
	{
		handleTranscoderFinished();
	}
}

void
LocalPlayer::handleDataAvailableFromTranscoder()
{
	_waitingDataFromTranscoder = false;
	feedAudioOutputFromTranscoder();
}

void
LocalPlayer::feedAudioOutputFromTranscoder()
{
	LMS_LOG(LOCALPLAYER, DEBUG) << "Feeding audio output from trasncoder...";
	assert(!_mutex.try_lock());

	if (!_transcoder)
	{
		LMS_LOG(LOCALPLAYER, DEBUG) << "Transcoder not ready yet";
		return;
	}

	std::size_t audioOutputCanWriteBytesCount {_audioOutput->getCanWriteBytes()};
	LMS_LOG(LOCALPLAYER, DEBUG) << "Audio output needs " << audioOutputCanWriteBytesCount << " bytes";

	if (audioOutputCanWriteBytesCount == 0)
		return;

	std::vector<unsigned char> buffer;
	buffer.resize(audioOutputCanWriteBytesCount);

	LMS_LOG(LOCALPLAYER, DEBUG) << "Reading up to " << buffer.size() << " bytes from transcoder";
	std::size_t nbTranscodedBytes {_transcoder->readSome(&buffer[0], buffer.size())};
	LMS_LOG(LOCALPLAYER, DEBUG) << "Got " << nbTranscodedBytes << " bytes from transcoder!";

	buffer.resize(nbTranscodedBytes);

	std::size_t nbWrittenBytes {_audioOutput->write(&buffer[0], buffer.size(), _nextWriteOffset)};
	_nextWriteOffset.reset();
	LMS_LOG(LOCALPLAYER, DEBUG) << "Written " << nbWrittenBytes << " bytes!";
	assert(nbWrittenBytes == nbTranscodedBytes);

	if (nbWrittenBytes < audioOutputCanWriteBytesCount)
	{
		LMS_LOG(LOCALPLAYER, DEBUG) << "not enough bytes from transcoder!";
		asyncWaitDataFromTranscoder();
	}
}

void
LocalPlayer::handleNeedDataFromAudioOutput()
{
	LMS_LOG(LOCALPLAYER, DEBUG) << "Some bytes needed from audio output!!";

	if (!_waitingDataFromTranscoder)
		feedAudioOutputFromTranscoder();
	else
		LMS_LOG(LOCALPLAYER, DEBUG) << "Already waiting for data from transcoder";
}


