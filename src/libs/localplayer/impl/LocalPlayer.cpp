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
#include "database/TrackList.hpp"
#include "database/User.hpp"
#include "localplayer/IAudioOutput.hpp"
#include "utils/Logger.hpp"

std::unique_ptr<ILocalPlayer> createLocalPlayer(Database::Db& db)
{
	return std::make_unique<LocalPlayer>(db);
}

LocalPlayer::LocalPlayer(Database::Db& db)
: _dbSession {db}
{

	createTrackListIfNeeded();

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
	std::unique_lock lock {_mutex};

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

	if (_playState == Status::PlayState::Paused)
	{
		_audioOutput->resume();
		_playState = Status::PlayState::Playing;

		if (!id)
			return;
	}

	if (id)
		_currentPlayQueueIdx = *id;

	if (!_currentPlayQueueIdx)
		_currentPlayQueueIdx = 0;

	const std::vector<Database::IdType> trackIds {getTrackIds()};

	while (*_currentPlayQueueIdx < trackIds.size())
	{
		if (!startPlayTrack(trackIds[*_currentPlayQueueIdx], offset))
		{
			(*_currentPlayQueueIdx)++;
			offset = {};
			continue;
		}

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

	LMS_LOG(LOCALPLAYER, DEBUG) << "No more song in playqueue> stopping";
	_currentPlayQueueIdx.reset();
	handleStop();
}

void
LocalPlayer::createTrackListIfNeeded()
{
	using namespace Database;

	static const char* trackListName {"__tracklist_localplayer__"};

	auto transaction {_dbSession.createUniqueTransaction()};

	TrackList::pointer trackList {TrackList::get(_dbSession, trackListName, TrackList::Type::Internal, {})};
	if (!trackList)
		trackList = TrackList::create(_dbSession, trackListName, TrackList::Type::Internal, false, {});


	_trackListId = trackList.id();
}

Database::TrackList::pointer
LocalPlayer::getTrackList()
{
	return Database::TrackList::getById(_dbSession, _trackListId);
}

std::vector<Database::IdType>
LocalPlayer::getTrackIds()
{
	assert(!_mutex.try_lock());
	auto transaction {_dbSession.createSharedTransaction()};
	
	return getTrackList()->getTrackIds();
}

std::optional<std::filesystem::path>
LocalPlayer::getTrackPath(Database::IdType trackId)
{
	using namespace Database;

	assert(!_mutex.try_lock());
	auto transaction {_dbSession.createSharedTransaction()};

	const Track::pointer track {Track::getById(_dbSession, trackId)};

	if (!track)
		return std::nullopt;

	return track->getPath();
}

bool
LocalPlayer::startPlayTrack(Database::IdType trackId, std::chrono::milliseconds offset)
{
	LMS_LOG(LOCALPLAYER, DEBUG) << "Playing track " << trackId;

	const auto trackPath {getTrackPath(trackId)};
	if (!trackPath)
		return false;

	Av::TranscodeParameters parameters {};
	parameters.stripMetadata = true;
	parameters.encoding = Av::Encoding::PCM_SIGNED_16_LE; // TODO
	parameters.offset = offset;

	_transcoder = std::make_unique<Av::Transcoder>(*trackPath, parameters);
	if (!_transcoder->start())
		return false;

	asyncWaitDataFromTranscoder();

	// TODO per playlist -> LmsApp->getUser().modify()->setCurPlayingTrackPos(pos);

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
	std::unique_lock lock {_mutex};

	if (_playState == Status::PlayState::Playing)
	{
		_audioOutput->pause();
		_playState = Status::PlayState::Paused;
	}
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

	auto transaction {_dbSession.createUniqueTransaction()};
	getTrackList().modify()->clear();
}

void
LocalPlayer::addTrack(Database::IdType trackId)
{
	using namespace Database;

	std::unique_lock lock {_mutex};

	auto transaction {_dbSession.createUniqueTransaction()};

	Track::pointer track {Track::getById(_dbSession, trackId)};
	if (track)
		TrackListEntry::create(_dbSession, track, getTrackList());
}

std::vector<Database::IdType>
LocalPlayer::getTracks()
{
	std::unique_lock lock {_mutex};

	return getTrackIds();
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


