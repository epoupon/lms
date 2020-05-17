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

#include <Wt/WText.h>
#include <Wt/WText.h>

#include "database/Cluster.hpp"
#include "database/Track.hpp"
#include "database/TrackList.hpp"
#include "database/User.hpp"
#include "recommendation/IEngine.hpp"
#include "utils/Logger.hpp"
#include "utils/Random.hpp"
#include "utils/Service.hpp"
#include "utils/String.hpp"

#include "resource/ImageResource.hpp"
#include "LmsApplication.hpp"
#include "MediaPlayer.hpp"
#include "TrackStringUtils.hpp"

namespace UserInterface {

PlayQueue::PlayQueue()
: Wt::WTemplate(Wt::WString::tr("Lms.PlayQueue.template"))
{
	addFunction("tr", &Wt::WTemplate::Functions::tr);

	{
		auto transaction {LmsApp->getDbSession().createSharedTransaction()};
		_repeatAll = LmsApp->getUser()->isRepeatAllSet();
		_radioMode = LmsApp->getUser()->isRadioSet();
	}

	Wt::WText* clearBtn = bindNew<Wt::WText>("clear-btn", Wt::WString::tr("Lms.PlayQueue.template.clear-btn"), Wt::TextFormat::XHTML);
	clearBtn->setToolTip(Wt::WString::tr("Lms.PlayQueue.clear"));
	clearBtn->clicked().connect([=]
	{
		clearTracks();
	});

	_entriesContainer = bindNew<Wt::WContainerWidget>("entries");

	_showMore = bindNew<Wt::WPushButton>("show-more", Wt::WString::tr("Lms.Explore.show-more"));
	_showMore->setHidden(true);
	_showMore->clicked().connect([=]
	{
		addSome();
		updateCurrentTrack(true);
	});

	Wt::WText* shuffleBtn = bindNew<Wt::WText>("shuffle-btn", Wt::WString::tr("Lms.PlayQueue.template.shuffle-btn"), Wt::TextFormat::XHTML);
	shuffleBtn->setToolTip(Wt::WString::tr("Lms.PlayQueue.shuffle"));
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

	_repeatBtn = bindNew<Wt::WText>("repeat-btn", Wt::WString::tr("Lms.PlayQueue.template.repeat-btn"), Wt::TextFormat::XHTML);
	_repeatBtn->setToolTip(Wt::WString::tr("Lms.PlayQueue.repeat"));
	_repeatBtn->clicked().connect([=]
	{
		_repeatAll = !_repeatAll;
		updateRepeatBtn();

		auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

		if (!LmsApp->getUser()->isDemo())
			LmsApp->getUser().modify()->setRepeatAll(_repeatAll);
	});
	updateRepeatBtn();

	_radioBtn = bindNew<Wt::WText>("radio-btn", Wt::WString::tr("Lms.PlayQueue.template.radio-btn"));
	_radioBtn->setToolTip(Wt::WString::tr("Lms.PlayQueue.radio-mode"));
	_radioBtn->clicked().connect([=]
	{
		_radioMode = !_radioMode;
		updateRadioBtn();

		auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

		if (!LmsApp->getUser()->isDemo())
			LmsApp->getUser().modify()->setRadio(_radioMode);
	});
	updateRadioBtn();

	_nbTracks = bindNew<Wt::WText>("nb-tracks");

	LmsApp->preQuit().connect([=]
	{
		auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

		if (LmsApp->getUser()->isDemo())
		{
			LMS_LOG(UI, DEBUG) << "Removing tracklist id " << _tracklistId;
			auto tracklist = Database::TrackList::getById(LmsApp->getDbSession(), _tracklistId);
			if (tracklist)
				tracklist.remove();
		}
	});

	{
		auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

		Database::TrackList::pointer trackList;

		if (!LmsApp->getUser()->isDemo())
		{
			LmsApp->getMediaPlayer()->settingsLoaded.connect([=]
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
			trackList = LmsApp->getUser()->getQueuedTrackList(LmsApp->getDbSession());
		}
		else
		{
			static const std::string currentPlayQueueName {"__current__playqueue__"};
			trackList = Database::TrackList::create(LmsApp->getDbSession(), currentPlayQueueName, Database::TrackList::Type::Internal, false, LmsApp->getUser());
		}

		_tracklistId = trackList.id();
	}

	updateInfo();
	addSome();
}

void
PlayQueue::updateRepeatBtn()
{
	_repeatBtn->toggleStyleClass("text-success", _repeatAll);
	_repeatBtn->toggleStyleClass("text-muted", !_repeatAll);
}

void
PlayQueue::updateRadioBtn()
{
	_radioBtn->toggleStyleClass("text-success",_radioMode);
	_radioBtn->toggleStyleClass("text-muted", !_radioMode);
}

Database::TrackList::pointer
PlayQueue::getTrackList() const
{
	return Database::TrackList::getById(LmsApp->getDbSession(), _tracklistId);
}

void
PlayQueue::clearTracks()
{
	{
		auto transaction {LmsApp->getDbSession().createUniqueTransaction()};
		getTrackList().modify()->clear();
	}

	_showMore->setHidden(true);
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

	Database::IdType trackId {};
	bool addRadioTrack {};
	std::optional<float> replayGain {};
	{
		auto transaction {LmsApp->getDbSession().createSharedTransaction()};

		Database::TrackList::pointer tracklist {getTrackList()};

		// If out of range, stop playing
		if (pos >= tracklist->getCount())
		{
			if (!_repeatAll || tracklist->getCount() == 0)
			{
				stop();
				return;
			}

			pos = 0;
		}

		// If last and radio mode, fill the next song
		if (_radioMode && pos == tracklist->getCount() - 1)
			addRadioTrack = true;

		_trackPos = pos;
		auto track = tracklist->getEntry(*_trackPos)->getTrack();

		trackId = track.id();

		replayGain = getReplayGain(pos, track);

		if (!LmsApp->getUser()->isDemo())
			LmsApp->getUser().modify()->setCurPlayingTrackPos(pos);
	}

	if (addRadioTrack)
		enqueueRadioTrack();

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
	if (!_trackPos || *_trackPos >= static_cast<std::size_t>(_entriesContainer->count()))
		return;

	Wt::WTemplate* entry {static_cast<Wt::WTemplate*>(_entriesContainer->widget(*_trackPos))};
	if (entry)
		entry->bindString("is-selected", selected ? "Lms-playqueue-selected" : "");
}

void
PlayQueue::enqueueTracks(const std::vector<Database::IdType>& trackIds)
{
	{
		auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

		auto tracklist = getTrackList();
		for (Database::IdType trackId : trackIds)
		{
			Database::Track::pointer track {Database::Track::getById(LmsApp->getDbSession(), trackId)};
			if (!track)
				continue;

			Database::TrackListEntry::create(LmsApp->getDbSession(), track, tracklist);
		}
	}

	updateInfo();
	addSome();
}

void
PlayQueue::enqueueTrack(Database::IdType trackId)
{
	enqueueTracks({trackId});
}

void
PlayQueue::processTracks(PlayQueueAction action, const std::vector<Database::IdType>& trackIds)
{
	switch (action)
	{
		case PlayQueueAction::AddLast:
			enqueueTracks(trackIds);
			LmsApp->notifyMsg(MsgType::Info, Wt::WString::trn("Lms.PlayQueue.nb-tracks-added", trackIds.size()).arg(trackIds.size()), std::chrono::milliseconds(2000));
			break;

		case PlayQueueAction::AddNext:
			break;

		case PlayQueueAction::Play:
			clearTracks();
			enqueueTracks(trackIds);
			loadTrack(0, true);

			LmsApp->notifyMsg(MsgType::Info, Wt::WString::trn("Lms.PlayQueue.nb-tracks-playing", trackIds.size()).arg(trackIds.size()), std::chrono::milliseconds(2000));
			break;

	}
}

void
PlayQueue::addSome()
{
	auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	auto tracklist = getTrackList();

	auto tracklistEntries = tracklist->getEntries(_entriesContainer->count(), 50);
	for (const Database::TrackListEntry::pointer& tracklistEntry : tracklistEntries)
	{
		const auto tracklistEntryId {tracklistEntry.id()};
		const auto track {tracklistEntry->getTrack()};

		Wt::WTemplate* entry = _entriesContainer->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.PlayQueue.template.entry"));

		entry->bindString("name", Wt::WString::fromUTF8(track->getName()), Wt::TextFormat::Plain);

		const auto artists {track->getArtists()};
		const auto release {track->getRelease()};

		if (!artists.empty() || release)
			entry->setCondition("if-has-artists-or-release", true);

		if (!artists.empty())
		{
			entry->setCondition("if-has-artists", true);

			Wt::WContainerWidget* artistContainer {entry->bindNew<Wt::WContainerWidget>("artists")};
			for (const auto& artist : artists)
			{
				Wt::WTemplate* a {artistContainer->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.PlayQueue.template.entry-artist"))};
				a->bindWidget("artist", LmsApplication::createArtistAnchor(artist));
			}
		}
		if (release)
		{
			entry->setCondition("if-has-release", true);
			entry->bindWidget("release", LmsApplication::createReleaseAnchor(release));
			{
				Wt::WAnchor* anchor = entry->bindWidget("cover", LmsApplication::createReleaseAnchor(release, false));
				auto cover = std::make_unique<Wt::WImage>();
				cover->setImageLink(LmsApp->getImageResource()->getReleaseUrl(release.id(), ImageResource::Size::Small));
				cover->setStyleClass("Lms-cover");
				anchor->setImage(std::move(cover));
			}
		}
		else
		{
			auto cover = entry->bindNew<Wt::WImage>("cover");
			cover->setImageLink(LmsApp->getImageResource()->getTrackUrl(track.id(), ImageResource::Size::Small));
			cover->setStyleClass("Lms-cover");
		}

		entry->bindString("duration", trackDurationToString(track->getDuration()), Wt::TextFormat::Plain);

		Wt::WText* playBtn = entry->bindNew<Wt::WText>("play-btn", Wt::WString::tr("Lms.PlayQueue.template.play-btn"), Wt::TextFormat::XHTML);
		playBtn->clicked().connect(std::bind([=]
		{
			auto pos = _entriesContainer->indexOf(entry);
			if (pos >= 0)
				loadTrack(pos, true);
		}));

		Wt::WText* delBtn = entry->bindNew<Wt::WText>("del-btn", Wt::WString::tr("Lms.PlayQueue.template.delete-btn"), Wt::TextFormat::XHTML);
		delBtn->clicked().connect(std::bind([=]
		{
			// Remove the entry n both the widget tree and the playqueue
			{
				auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

				Database::TrackListEntry::pointer entryToRemove {Database::TrackListEntry::getById(LmsApp->getDbSession(), tracklistEntryId)};
				entryToRemove.remove();
			}

			if (_trackPos)
			{
				auto pos {_entriesContainer->indexOf(entry)};
				if (pos > 0 && *_trackPos >= static_cast<std::size_t>(pos))
					(*_trackPos)--;
			}

			_entriesContainer->removeWidget(entry);

			updateInfo();
		}));

	}

	_showMore->setHidden(static_cast<std::size_t>(_entriesContainer->count()) >= tracklist->getCount());
}

void
PlayQueue::enqueueRadioTrack()
{
	const std::vector<Database::IdType> trackToAddIds {ServiceProvider<Recommendation::IEngine>::get()->getSimilarTracksFromTrackList(LmsApp->getDbSession(), _tracklistId, 1)};
	enqueueTracks(trackToAddIds);
}

std::optional<float>
PlayQueue::getReplayGain(std::size_t pos, const Database::Track::pointer& track) const
{
	const auto& settings {LmsApp->getMediaPlayer()->getSettings()};
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

