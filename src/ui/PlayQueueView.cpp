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

#include <random>

#include <Wt/WAnchor.h>
#include <Wt/WText.h>

#include "utils/Logger.hpp"

#include "database/TrackList.hpp"

#include "LmsApplication.hpp"


namespace UserInterface {

PlayQueue::PlayQueue()
: Wt::WTemplate(Wt::WString::tr("Lms.PlayQueue.template"))
{
	addFunction("tr", &Wt::WTemplate::Functions::tr);

	Wt::WText* clearBtn = bindNew<Wt::WText>("clear-btn", Wt::WString::tr("Lms.PlayQueue.clear"), Wt::TextFormat::XHTML);

	_entriesContainer = bindNew<Wt::WContainerWidget>("entries");

	_showMore = bindNew<Wt::WPushButton>("show-more", Wt::WString::tr("Lms.Explore.show-more"));
	_showMore->setHidden(true);

	Wt::WText* shuffleBtn = bindNew<Wt::WText>("shuffle-btn", Wt::WString::tr("Lms.PlayQueue.shuffle"), Wt::TextFormat::XHTML);
	_radioMode = bindNew<Wt::WCheckBox>("radio-mode", Wt::WString::tr("Lms.PlayQueue.radio-mode"));
	_nbTracks = bindNew<Wt::WText>("nb-tracks");

	clearBtn->clicked().connect([=]
	{
		clearTracks();
	});

	shuffleBtn->clicked().connect([=]
	{
		{
			Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

			getTrackList().modify()->shuffle();
		}
		_entriesContainer->clear();
		addSome();
	});

	_showMore->clicked().connect([=]
	{
		addSome();
		updateCurrentTrack(true);
	});

	LmsApp->preQuit().connect([=]
	{
		if (_tracklistId)
		{
			LMS_LOG(UI, DEBUG) << "Removing tracklist id " << *_tracklistId;

			Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

			auto tracklist = Database::TrackList::getById(LmsApp->getDboSession(), *_tracklistId);
			if (tracklist)
				tracklist.remove();
		}
	});

	updateInfo();
	addSome();

	if (!LmsApp->getUser()->isDemo())
	{
		LmsApp->post([=]
		{
			load(LmsApp->getUser()->getCurPlayingTrackPos(), false);
		});
	}
}

Database::TrackList::pointer
PlayQueue::getTrackList()
{
	static const std::string currentPlayQueueName = "__current__playqueue__";
	Database::TrackList::pointer res;

	if (LmsApp->getUser()->isDemo())
	{
		if (!_tracklistId)
		{
			res = Database::TrackList::create(LmsApp->getDboSession(), currentPlayQueueName, false, LmsApp->getUser());
			LmsApp->getDboSession().flush();
			_tracklistId = res.id();
			return res;
		}

		return Database::TrackList::getById(LmsApp->getDboSession(), *_tracklistId);
	}

	return LmsApp->getUser()->getQueuedTrackList();
}

void
PlayQueue::clearTracks()
{
	Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

	getTrackList().modify()->clear();
	_showMore->setHidden(true);
	_entriesContainer->clear();
	updateInfo();
}

void
PlayQueue::stop()
{
	updateCurrentTrack(false);
	_trackPos.reset();
	trackUnload.emit();
}

void
PlayQueue::load(std::size_t pos, bool play)
{
	updateCurrentTrack(false);

	Database::IdType trackId;
	{
		Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

		auto tracklist = getTrackList();

		// If out of range, stop playing
		if (pos >= tracklist->getCount())
		{
			stop();
			return;
		}

		// If last and radio mode, fill the next song
		if (_radioMode->checkState() == Wt::CheckState::Checked && pos == tracklist->getCount() - 1)
			addRadioTrack();

		_trackPos = pos;
		auto track = tracklist->getEntry(*_trackPos)->getTrack();

		trackId = track.id();

		updateCurrentTrack(true);

		if (!LmsApp->getUser()->isDemo())
			LmsApp->getUser().modify()->setCurPlayingTrackPos(pos);
	}

	loadTrack.emit(trackId, play);
}

void
PlayQueue::playPrevious()
{
	if (!_trackPos)
		return;

	if (*_trackPos == 0)
		stop();
	else
		load(*_trackPos - 1, true);
}

void
PlayQueue::playNext()
{
	if (!_trackPos)
	{
		load(0, true);
		return;
	}

	load(*_trackPos + 1, true);
}

void
PlayQueue::updateInfo()
{
	Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

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
PlayQueue::enqueueTracks(const std::vector<Database::Track::pointer>& tracks)
{
	// Use a "session" playqueue in order to store the current playqueue
	// so that the user can disconnect and get its playqueue back

	auto tracklist = getTrackList();

	for (auto track : tracks)
		Database::TrackListEntry::create(LmsApp->getDboSession(), track, tracklist);

	updateInfo();
	addSome();
}

void
PlayQueue::enqueueTrack(Database::Track::pointer track)
{
	enqueueTracks(std::vector<Database::Track::pointer>(1, track));
}

void
PlayQueue::addTracks(const std::vector<Database::Track::pointer>& tracks)
{
	enqueueTracks(tracks);
	LmsApp->notifyMsg(MsgType::Info, Wt::WString::tr("Lms.PlayQueue.nb-tracks-added").arg(tracks.size()), std::chrono::milliseconds(2000));
}

void
PlayQueue::playTracks(const std::vector<Database::Track::pointer>& tracks)
{
	Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

	clearTracks();
	enqueueTracks(tracks);
	load(0, true);

	LmsApp->notifyMsg(MsgType::Info, Wt::WString::tr("Lms.PlayQueue.nb-tracks-playing").arg(tracks.size()), std::chrono::milliseconds(2000));
}


void
PlayQueue::addSome()
{
	Wt::Dbo::Transaction transaction (LmsApp->getDboSession());

	auto tracklist = getTrackList();

	auto tracklistEntries = tracklist->getEntries(_entriesContainer->count(), 50);
	for (auto tracklistEntry : tracklistEntries)
	{
		auto tracklistEntryId = tracklistEntry.id();
		auto track = tracklistEntry->getTrack();

		Wt::WTemplate* entry = _entriesContainer->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.PlayQueue.template.entry"));

		entry->bindString("name", Wt::WString::fromUTF8(track->getName()), Wt::TextFormat::Plain);

		auto artist = track->getArtist();
		if (artist)
		{
			entry->setCondition("if-has-artist", true);
			entry->bindWidget("artist-name", LmsApplication::createArtistAnchor(track->getArtist()));
		}
		auto release = track->getRelease();
		if (release)
		{
			entry->setCondition("if-has-release", true);
			entry->bindWidget("release-name", LmsApplication::createReleaseAnchor(track->getRelease()));
		}

		Wt::WText* playBtn = entry->bindNew<Wt::WText>("play-btn", Wt::WString::tr("Lms.PlayQueue.play"), Wt::TextFormat::XHTML);
		playBtn->clicked().connect(std::bind([=]
		{
			auto pos = _entriesContainer->indexOf(entry);
			if (pos >= 0)
				load(pos, true);
		}));

		Wt::WText* delBtn = entry->bindNew<Wt::WText>("del-btn", Wt::WString::tr("Lms.PlayQueue.delete"), Wt::TextFormat::XHTML);
		delBtn->clicked().connect(std::bind([=]
		{
			// Remove the entry n both the widget tree and the playqueue
			{
				Wt::Dbo::Transaction transaction (LmsApp->getDboSession());

				auto entryToRemove = Database::TrackListEntry::getById(LmsApp->getDboSession(), tracklistEntryId);
				entryToRemove.remove();
			}

			if (_trackPos)
			{
				auto pos = _entriesContainer->indexOf(entry);
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
PlayQueue::addRadioTrack()
{
	auto now = std::chrono::system_clock::now();
	std::mt19937 randGenerator(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());

	auto tracklist = getTrackList();

	std::set<Database::IdType> trackIds;
	{
		auto ids = tracklist->getTrackIds();
		trackIds = std::set<Database::IdType>(ids.begin(), ids.end());
	}

	// Get all the tracks of the tracklist, get the cluster that is mostly used
	// and reuse it to get the next track
	auto clusters = tracklist->getClusters();
	if (clusters.empty())
		return;

	for (auto cluster : clusters)
	{
		std::set<Database::IdType> clusterTrackIds = cluster->getTrackIds();

		std::set<Database::IdType> candidateTrackIds;
		std::set_difference(clusterTrackIds.begin(), clusterTrackIds.end(),
				trackIds.begin(), trackIds.end(),
				std::inserter(candidateTrackIds, candidateTrackIds.end()));

		if (candidateTrackIds.empty())
			continue;

		std::uniform_int_distribution<int> dist(0, candidateTrackIds.size() - 1);

		auto trackToAdd = Database::Track::getById(LmsApp->getDboSession(), *std::next(candidateTrackIds.begin(), dist(randGenerator)));
		enqueueTrack(trackToAdd);

		return;
	}

	LMS_LOG(UI, INFO) << "No more track to be added!";
}

} // namespace UserInterface

