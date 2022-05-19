/*
 * Copyright (C) 2018 Emeric Poupon
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

#include "PlayQueue.hpp"

#include <Wt/WCheckBox.h>
#include <Wt/WPushButton.h>

#include "services/database/Cluster.hpp"
#include "services/database/Release.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "services/database/TrackList.hpp"
#include "services/database/User.hpp"
#include "services/scrobbling/IScrobblingService.hpp"
#include "services/recommendation/IRecommendationService.hpp"
#include "utils/Logger.hpp"
#include "utils/Random.hpp"
#include "utils/Service.hpp"
#include "utils/String.hpp"

#include "common/InfiniteScrollingContainer.hpp"
#include "resource/DownloadResource.hpp"
#include "LmsApplication.hpp"
#include "MediaPlayer.hpp"
#include "Utils.hpp"

namespace UserInterface {

PlayQueue::PlayQueue()
: Template {Wt::WString::tr("Lms.PlayQueue.template")}
{
	addFunction("id", &Wt::WTemplate::Functions::id);
	addFunction("tr", &Wt::WTemplate::Functions::tr);

	Wt::WText* clearBtn = bindNew<Wt::WText>("clear-btn", Wt::WString::tr("Lms.PlayQueue.template.clear-btn"), Wt::TextFormat::XHTML);
	clearBtn->clicked().connect([=]
	{
		clearTracks();
	});

	_entriesContainer = bindNew<InfiniteScrollingContainer>("entries");
	_entriesContainer->onRequestElements.connect([this]
	{
		addSome();
		updateCurrentTrack(true);
	});

	Wt::WText* shuffleBtn = bindNew<Wt::WText>("shuffle-btn", Wt::WString::tr("Lms.PlayQueue.template.shuffle-btn"), Wt::TextFormat::XHTML);
	shuffleBtn->clicked().connect([=]
	{
		{
			auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

			Database::TrackList::pointer trackList {getTrackList()};
			auto entries {trackList->getEntries()};
			Random::shuffleContainer(entries);

			getTrackList().modify()->clear();
			for (const auto& entry : entries)
				Database::TrackListEntry::create(LmsApp->getDbSession(), entry->getTrack(), trackList);
		}
		_entriesContainer->clear();
		addSome();
	});

	_repeatBtn = bindNew<Wt::WCheckBox>("repeat-btn");
	_repeatBtn->clicked().connect([=]
	{
		auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

		if (!LmsApp->getUser()->isDemo())
			LmsApp->getUser().modify()->setRepeatAll(isRepeatAllSet());
	});
	{
		auto transaction {LmsApp->getDbSession().createSharedTransaction()};
		if (LmsApp->getUser()->isRepeatAllSet())
			_repeatBtn->setCheckState(Wt::CheckState::Checked);
	}

	_radioBtn = bindNew<Wt::WCheckBox>("radio-btn");
	_radioBtn->clicked().connect([=]
	{
		auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

		if (!LmsApp->getUser()->isDemo())
			LmsApp->getUser().modify()->setRadio(isRadioModeSet());
	});
	{
		auto transaction {LmsApp->getDbSession().createSharedTransaction()};
		if (LmsApp->getUser()->isRadioSet())
			_radioBtn->setCheckState(Wt::CheckState::Checked);
	}

	_nbTracks = bindNew<Wt::WText>("nb-tracks");

	LmsApp->preQuit().connect([=]
	{
		auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

		if (LmsApp->getUser()->isDemo())
		{
			LMS_LOG(UI, DEBUG) << "Removing tracklist id " << _tracklistId.toString();
			auto tracklist = Database::TrackList::find(LmsApp->getDbSession(), _tracklistId);
			if (tracklist)
				tracklist.remove();
		}
	});

	{
		auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

		Database::TrackList::pointer trackList;

		if (!LmsApp->getUser()->isDemo())
		{
			LmsApp->getMediaPlayer().settingsLoaded.connect([=]
			{
				if (_mediaPlayerSettingsLoaded)
					return;

				_mediaPlayerSettingsLoaded = true;

				std::size_t trackPos {};

				{
					auto transaction {LmsApp->getDbSession().createSharedTransaction()};
					trackPos = LmsApp->getUser()->getCurPlayingTrackPos();
				}

				loadTrack(trackPos, false);
			});

			static const std::string queuedListName {"__queued_tracks__"};
			trackList = Database::TrackList::find(LmsApp->getDbSession(), queuedListName, Database::TrackList::Type::Internal, LmsApp->getUserId());
			if (!trackList)
				trackList = Database::TrackList::create(LmsApp->getDbSession(), queuedListName, Database::TrackList::Type::Internal, false, LmsApp->getUser());
		}
		else
		{
			static const std::string currentPlayQueueName {"__current__playqueue__"};
			trackList = Database::TrackList::create(LmsApp->getDbSession(), currentPlayQueueName, Database::TrackList::Type::Internal, false, LmsApp->getUser());
		}

		_tracklistId = trackList->getId();
	}

	updateInfo();
	addSome();
}

bool
PlayQueue::isRepeatAllSet() const
{
	return _repeatBtn->checkState() == Wt::CheckState::Checked;
}

bool
PlayQueue::isRadioModeSet() const
{
	return _radioBtn->checkState() == Wt::CheckState::Checked;
}

Database::TrackList::pointer
PlayQueue::getTrackList() const
{
	return Database::TrackList::find(LmsApp->getDbSession(), _tracklistId);
}

bool
PlayQueue::isFull() const
{
	auto transaction {LmsApp->getDbSession().createSharedTransaction()};
	return getTrackList()->getCount() == _nbMaxEntries;
}

void
PlayQueue::clearTracks()
{
	{
		auto transaction {LmsApp->getDbSession().createUniqueTransaction()};
		getTrackList().modify()->clear();
	}

	_entriesContainer->clear();
	updateInfo();
}

void
PlayQueue::stop()
{
	updateCurrentTrack(false);
	_trackPos.reset();
	trackUnselected.emit();
}

void
PlayQueue::loadTrack(std::size_t pos, bool play)
{
	updateCurrentTrack(false);

	Database::TrackId trackId {};
	bool addRadioTrack {};
	std::optional<float> replayGain {};
	{
		auto transaction {LmsApp->getDbSession().createSharedTransaction()};

		Database::TrackList::pointer tracklist {getTrackList()};

		// If out of range, stop playing
		if (pos >= tracklist->getCount())
		{
			if (!isRepeatAllSet() || tracklist->getCount() == 0)
			{
				stop();
				return;
			}

			pos = 0;
		}

		// If last and radio mode, fill the next song
		if (isRadioModeSet() && pos == tracklist->getCount() - 1)
			addRadioTrack = true;

		_trackPos = pos;
		auto track = tracklist->getEntry(*_trackPos)->getTrack();

		trackId = track->getId();

		replayGain = getReplayGain(pos, track);

		if (!LmsApp->getUser()->isDemo())
			LmsApp->getUser().modify()->setCurPlayingTrackPos(pos);
	}

	if (addRadioTrack)
		enqueueRadioTracks();

	updateCurrentTrack(true);

	trackSelected.emit(trackId, play, replayGain ? *replayGain : 0);
}

void
PlayQueue::playPrevious()
{
	if (!_trackPos)
		return;

	if (*_trackPos == 0)
		stop();
	else
		loadTrack(*_trackPos - 1, true);
}

void
PlayQueue::playNext()
{
	if (!_trackPos)
	{
		loadTrack(0, true);
		return;
	}

	loadTrack(*_trackPos + 1, true);
}

void
PlayQueue::updateInfo()
{
	auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	_nbTracks->setText(Wt::WString::tr("Lms.PlayQueue.nb-tracks").arg(static_cast<unsigned>(getTrackList()->getCount())));
}

void
PlayQueue::updateCurrentTrack(bool selected)
{
	if (!_trackPos || *_trackPos >= static_cast<std::size_t>(_entriesContainer->getCount()))
		return;

	Template* entry {static_cast<Template*>(_entriesContainer->getWidget(*_trackPos))};
	if (!entry)
		return;

	entry->toggleStyleClass("Lms-entry-playing", selected);
}

std::size_t
PlayQueue::enqueueTracks(const std::vector<Database::TrackId>& trackIds)
{
	std::size_t nbTracksQueued {};

	{
		auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

		auto tracklist {getTrackList()};

		std::size_t nbTracksToEnqueue {tracklist->getCount() + trackIds.size() > _nbMaxEntries ? _nbMaxEntries - tracklist->getCount() : trackIds.size()};
		for (const Database::TrackId trackId : trackIds)
		{
			Database::Track::pointer track {Database::Track::find(LmsApp->getDbSession(), trackId)};
			if (!track)
				continue;

			if (nbTracksQueued == nbTracksToEnqueue)
				break;

			Database::TrackListEntry::create(LmsApp->getDbSession(), track, tracklist);
			nbTracksQueued++;
		}
	}

	updateInfo();
	addSome();

	return nbTracksQueued;
}

void
PlayQueue::processTracks(PlayQueueAction action, const std::vector<Database::TrackId>& trackIds)
{
	std::size_t nbAddedTracks {};

	switch (action)
	{
		case PlayQueueAction::PlayLast:
			nbAddedTracks = enqueueTracks(trackIds);
			if (!_trackPos)
				loadTrack(0, true);
			break;

		case PlayQueueAction::Play:
			clearTracks();
			nbAddedTracks = enqueueTracks(trackIds);
			loadTrack(0, true);

			break;

		case PlayQueueAction::PlayShuffled:
		{
			clearTracks();
			{
				std::vector<Database::TrackId> shuffledTrackIds {trackIds};
				Random::shuffleContainer(shuffledTrackIds);
				nbAddedTracks = enqueueTracks(shuffledTrackIds);
			}
			loadTrack(0, true);
			break;
		}
	}

	if (nbAddedTracks > 0)
		LmsApp->notifyMsg(Notification::Type::Info, Wt::WString::tr("Lms.PlayQueue.playqueue"), Wt::WString::trn("Lms.PlayQueue.nb-tracks-added", nbAddedTracks).arg(nbAddedTracks), std::chrono::milliseconds(2000));

	if (isFull())
		LmsApp->notifyMsg(Notification::Type::Warning, Wt::WString::tr("Lms.PlayQueue.playqueue"), Wt::WString::tr("Lms.PlayQueue.playqueue-full"), std::chrono::milliseconds(2000));
}

void
PlayQueue::addSome()
{
	auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	auto tracklist = getTrackList();

	auto tracklistEntries = tracklist->getEntries(_entriesContainer->getCount(), _batchSize);
	for (const Database::TrackListEntry::pointer& tracklistEntry : tracklistEntries)
		addEntry(tracklistEntry);

	_entriesContainer->setHasMore(_entriesContainer->getCount() < tracklist->getCount());
}

void
PlayQueue::addEntry(const Database::TrackListEntry::pointer& tracklistEntry)
{
	const Database::TrackListEntryId tracklistEntryId {tracklistEntry->getId()};
	const auto track {tracklistEntry->getTrack()};
	const Database::TrackId trackId {track->getId()};

	Template* entry {_entriesContainer->addNew<Template>(Wt::WString::tr("Lms.PlayQueue.template.entry"))};
	entry->addFunction("id", &Wt::WTemplate::Functions::id);

	entry->bindString("name", Wt::WString::fromUTF8(track->getName()), Wt::TextFormat::Plain);

	const auto release {track->getRelease()};
	if (release)
	{
		entry->setCondition("if-has-release", true);
		entry->bindWidget("release", LmsApplication::createReleaseAnchor(release));
		{
			Wt::WAnchor* anchor {entry->bindWidget("cover", LmsApplication::createReleaseAnchor(release, false))};
			auto cover {Utils::createCover(release->getId(), CoverResource::Size::Small)};
			cover->addStyleClass("Lms-cover-track"); // HACK
			anchor->setImage(std::move(cover));
		}
	}
	else
	{
		auto cover {entry->bindWidget<Wt::WImage>("cover", Utils::createCover(track->getId(), CoverResource::Size::Small))};
		cover->addStyleClass("Lms-cover-track");
	}

	entry->bindString("duration", Utils::durationToString(track->getDuration()), Wt::TextFormat::Plain);

	Wt::WText* playBtn {entry->bindNew<Wt::WText>("play-btn", Wt::WString::tr("Lms.PlayQueue.template.play-btn"), Wt::TextFormat::XHTML)};
	playBtn->clicked().connect([=]
	{
		const std::optional<std::size_t> pos {_entriesContainer->getIndexOf(*entry)};
		if (pos)
			loadTrack(*pos, true);
	});

	Wt::WText* delBtn {entry->bindNew<Wt::WText>("del-btn", Wt::WString::tr("Lms.PlayQueue.template.delete-btn"), Wt::TextFormat::XHTML)};
	delBtn->clicked().connect([=]
	{
		// Remove the entry n both the widget tree and the playqueue
		{
			auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

			Database::TrackListEntry::pointer entryToRemove {Database::TrackListEntry::getById(LmsApp->getDbSession(), tracklistEntryId)};
			entryToRemove.remove();
		}

		if (_trackPos)
		{
			const std::optional<std::size_t> pos {_entriesContainer->getIndexOf(*entry)};
			if (pos && *_trackPos >= *pos)
				(*_trackPos)--;
		}

		_entriesContainer->remove(*entry);

		updateInfo();
	});

	entry->bindNew<Wt::WText>("more-btn", Wt::WString::tr("Lms.PlayQueue.template.more-btn"), Wt::TextFormat::XHTML);

	auto isStarred {[=] { return Service<Scrobbling::IScrobblingService>::get()->isStarred(LmsApp->getUserId(), trackId); }};

	Wt::WPushButton* starBtn {entry->bindNew<Wt::WPushButton>("star", Wt::WString::tr(isStarred() ? "Lms.Explore.unstar" : "Lms.Explore.star"))};
	starBtn->clicked().connect([=]
	{
		auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

		if (isStarred())
		{
			Service<Scrobbling::IScrobblingService>::get()->unstar(LmsApp->getUserId(), trackId);
			starBtn->setText(Wt::WString::tr("Lms.Explore.star"));
		}
		else
		{
			Service<Scrobbling::IScrobblingService>::get()->star(LmsApp->getUserId(), trackId);
			starBtn->setText(Wt::WString::tr("Lms.Explore.unstar"));
		}
	});

	entry->bindNew<Wt::WPushButton>("download", Wt::WString::tr("Lms.Explore.download"))
		->setLink(Wt::WLink {std::make_unique<DownloadTrackResource>(trackId)});
}

void
PlayQueue::enqueueRadioTracks()
{
	const auto similarTrackIds {Service<Recommendation::IRecommendationService>::get()->findSimilarTracksFromTrackList(_tracklistId, 3)};

	std::vector<Database::TrackId> trackToAddIds(std::cbegin(similarTrackIds), std::cend(similarTrackIds));
	Random::shuffleContainer(trackToAddIds);
	enqueueTracks(trackToAddIds);
}

std::optional<float>
PlayQueue::getReplayGain(std::size_t pos, const Database::Track::pointer& track) const
{
	const auto& settings {LmsApp->getMediaPlayer().getSettings()};
	if (!settings)
		return std::nullopt;

	std::optional<MediaPlayer::Gain> gain;

	switch (settings->replayGain.mode)
	{
		case MediaPlayer::Settings::ReplayGain::Mode::None:
			return std::nullopt;

		case MediaPlayer::Settings::ReplayGain::Mode::Track:
			gain = track->getTrackReplayGain();
			break;

		case MediaPlayer::Settings::ReplayGain::Mode::Release:
			gain = track->getReleaseReplayGain();
			if (!gain)
				gain = track->getTrackReplayGain();
			break;

		case MediaPlayer::Settings::ReplayGain::Mode::Auto:
		{
			const auto trackList {getTrackList()};
			const auto prevEntry {pos > 0 ? trackList->getEntry(pos - 1) : Database::TrackListEntry::pointer {}};
			const auto nextEntry {trackList->getEntry(pos + 1)};
			const Database::Track::pointer prevTrack {prevEntry ? prevEntry->getTrack() : Database::Track::pointer {}};
			const Database::Track::pointer nextTrack {nextEntry ? nextEntry->getTrack() : Database::Track::pointer {}};

			if ((prevTrack && prevTrack->getRelease() && prevTrack->getRelease() == track->getRelease())
				||
				(nextTrack && nextTrack->getRelease() && nextTrack->getRelease() == track->getRelease()))
			{
				gain = track->getReleaseReplayGain();
				if (!gain)
					gain = track->getTrackReplayGain();
			}
			else
			{
				gain = track->getTrackReplayGain();
			}
			break;
		}
	}

	if (gain)
		return *gain + settings->replayGain.preAmpGain;

	return settings->replayGain.preAmpGainIfNoInfo;
}

} // namespace UserInterface

