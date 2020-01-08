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

#include "PlayQueueView.hpp"

#include <Wt/WText.h>
#include <Wt/WText.h>

#include "database/Cluster.hpp"
#include "database/Track.hpp"
#include "database/TrackList.hpp"
#include "database/User.hpp"
#include "similarity/SimilaritySearcher.hpp"
#include "utils/Logger.hpp"
#include "utils/Service.hpp"
#include "utils/Utils.hpp"
#include "TrackStringUtils.hpp"
#include "LmsApplication.hpp"

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
			shuffleContainer(entries);

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
			LmsApp->post([=]
			{
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
	_repeatBtn->toggleStyleClass("Lms-playqueue-btn-selected", _repeatAll);
}

void
PlayQueue::updateRadioBtn()
{
	_radioBtn->toggleStyleClass("Lms-playqueue-btn-selected",_radioMode);
}

Database::TrackList::pointer
PlayQueue::getTrackList()
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
	{
		auto transaction {LmsApp->getDbSession().createSharedTransaction()};

		Database::TrackList::pointer tracklist {getTrackList()};

		// If out of range, stop playing
		if (pos >= tracklist->getCount())
		{
			if (!_repeatAll)
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

		if (!LmsApp->getUser()->isDemo())
			LmsApp->getUser().modify()->setCurPlayingTrackPos(pos);
	}

	if (addRadioTrack)
		enqueueRadioTrack();

	updateCurrentTrack(true);

	trackSelected.emit(trackId, play);
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

	auto track = _entriesContainer->widget(*_trackPos);
	if (track)
	{
		if (selected)
			track->addStyleClass("Lms-playqueue-selected");
		else
			track->removeStyleClass("Lms-playqueue-selected");
	}
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
PlayQueue::addTracks(const std::vector<Database::IdType>& trackIds)
{
	enqueueTracks(trackIds);
	LmsApp->notifyMsg(MsgType::Info, Wt::WString::trn("Lms.PlayQueue.nb-tracks-added", trackIds.size()).arg(trackIds.size()), std::chrono::milliseconds(2000));
}

void
PlayQueue::playTracks(const std::vector<Database::IdType>& trackIds)
{
	clearTracks();
	enqueueTracks(trackIds);
	loadTrack(0, true);

	LmsApp->notifyMsg(MsgType::Info, Wt::WString::trn("Lms.PlayQueue.nb-tracks-playing", trackIds.size()).arg(trackIds.size()), std::chrono::milliseconds(2000));
}

void
PlayQueue::addSome()
{
	auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	auto tracklist = getTrackList();

	auto tracklistEntries = tracklist->getEntries(_entriesContainer->count(), 50);
	for (const Database::TrackListEntry::pointer& tracklistEntry : tracklistEntries)
	{
		auto tracklistEntryId = tracklistEntry.id();
		auto track = tracklistEntry->getTrack();

		Wt::WTemplate* entry = _entriesContainer->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.PlayQueue.template.entry"));

		entry->bindString("name", Wt::WString::fromUTF8(track->getName()), Wt::TextFormat::Plain);

		auto artists {track->getArtists()};
		auto release = track->getRelease();

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
			entry->bindWidget("release", LmsApplication::createReleaseAnchor(track->getRelease()));
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
	const std::vector<Database::IdType> trackToAddIds {getService<Similarity::Searcher>()->getSimilarTracksFromTrackList(LmsApp->getDbSession(), _tracklistId, 1)};
	enqueueTracks(trackToAddIds);
}

} // namespace UserInterface

