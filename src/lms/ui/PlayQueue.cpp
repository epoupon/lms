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

#include <Wt/WPopupMenu.h>
#include <Wt/WText.h>

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
#include "resource/CoverResource.hpp"
#include "resource/DownloadResource.hpp"
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

	auto setToolTip = [](Wt::WWidget& widget, Wt::WString title)
	{
		widget.setAttributeValue("data-toggle", "tooltip");
		widget.setAttributeValue("data-placement", "bottom");
		widget.setAttributeValue("title", std::move(title));
	};

	Wt::WText* clearBtn = bindNew<Wt::WText>("clear-btn", Wt::WString::tr("Lms.PlayQueue.template.clear-btn"), Wt::TextFormat::XHTML);
	setToolTip(*clearBtn, Wt::WString::tr("Lms.PlayQueue.clear"));
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
	setToolTip(*shuffleBtn, Wt::WString::tr("Lms.PlayQueue.shuffle"));
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
	setToolTip(*_repeatBtn, Wt::WString::tr("Lms.PlayQueue.repeat"));
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
	setToolTip(*_radioBtn, Wt::WString::tr("Lms.PlayQueue.radio-mode"));
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

	Wt::WTemplate* entry {static_cast<Wt::WTemplate*>(_entriesContainer->getWidget(*_trackPos))};
	if (entry)
		entry->bindString("is-selected", selected ? "Lms-playqueue-selected" : "");
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

	Wt::WTemplate* entry = _entriesContainer->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.PlayQueue.template.entry"));

	entry->bindString("is-selected", "");
	entry->bindString("name", Wt::WString::fromUTF8(track->getName()), Wt::TextFormat::Plain);

	const auto artists {track->getArtists({Database::TrackArtistLinkType::Artist})};
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
			cover->setImageLink(LmsApp->getCoverResource()->getReleaseUrl(release->getId(), CoverResource::Size::Large));
			cover->setStyleClass("img-fluid");
			cover->setAttributeValue("onload", LmsApp->javaScriptClass() + ".onLoadCover(this)");
			anchor->setImage(std::move(cover));
		}
	}
	else
	{
		auto cover = entry->bindNew<Wt::WImage>("cover");
		cover->setImageLink(LmsApp->getCoverResource()->getTrackUrl(track->getId(), CoverResource::Size::Large));
		cover->setStyleClass("img-fluid");
		cover->setAttributeValue("onload", LmsApp->javaScriptClass() + ".onLoadCover(this)");
	}

	entry->bindString("duration", durationToString(track->getDuration()), Wt::TextFormat::Plain);

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

	Wt::WText* moreBtn {entry->bindNew<Wt::WText>("more-btn", Wt::WString::tr("Lms.PlayQueue.template.more-btn"), Wt::TextFormat::XHTML)};
	moreBtn->clicked().connect([=]
	{
		Wt::WPopupMenu* popup {LmsApp->createPopupMenu()};

		const bool isStarred {Service<Scrobbling::IScrobblingService>::get()->isStarred(LmsApp->getUserId(), trackId)};
		popup->addItem(Wt::WString::tr(isStarred ? "Lms.Explore.unstar" : "Lms.Explore.star"))
			->triggered().connect(moreBtn, [=]
			{
				if (isStarred)
					Service<Scrobbling::IScrobblingService>::get()->unstar(LmsApp->getUserId(), trackId);
				else
					Service<Scrobbling::IScrobblingService>::get()->star(LmsApp->getUserId(), trackId);
			});
		popup->addItem(Wt::WString::tr("Lms.Explore.download"))
			->setLink(Wt::WLink {std::make_unique<DownloadTrackResource>(trackId)});

		popup->popup(moreBtn);
	});
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

