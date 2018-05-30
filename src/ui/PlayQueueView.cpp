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
#include <Wt/WImage.h>
#include <Wt/WText.h>

#include "utils/Logger.hpp"

#include "database/Playlist.hpp"

#include "LmsApplication.hpp"

namespace UserInterface {

static const std::string currentPlayQueueName = "__current__playqueue__";

PlayQueue::PlayQueue()
: Wt::WTemplate(Wt::WString::tr("Lms.PlayQueue.template"))
{
	addFunction("tr", &Wt::WTemplate::Functions::tr);

	Wt::WText* clearBtn = bindNew<Wt::WText>("clear-btn", Wt::WString::tr("Lms.PlayQueue.clear"), Wt::TextFormat::XHTML);

	_entriesContainer = bindNew<Wt::WContainerWidget>("entries");

	_showMore = bindNew<Wt::WTemplate>("show-more", Wt::WString::tr("Lms.Explore.show-more"));
	_showMore->addFunction("tr", &Wt::WTemplate::Functions::tr);
	_showMore->setHidden(true);

	_radioMode = bindNew<Wt::WCheckBox>("radio-mode", Wt::WString::tr("Lms.PlayQueue.radio-mode"));

	_nbTracks = bindNew<Wt::WText>("nb-tracks");

	{
		Wt::Dbo::Transaction transaction (LmsApp->getDboSession());

		auto playlist = Database::Playlist::get(LmsApp->getDboSession(), currentPlayQueueName, LmsApp->getCurrentUser());
		if (!playlist)
			playlist = Database::Playlist::create(LmsApp->getDboSession(), currentPlayQueueName, false, LmsApp->getCurrentUser());

		_trackPos = LmsApp->getCurrentUser()->getCurPlayingTrackPos();
	}

	clearBtn->clicked().connect(std::bind([=]
	{
		{
			Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

			auto playlist = Database::Playlist::get(LmsApp->getDboSession(), currentPlayQueueName, LmsApp->getCurrentUser());
			playlist.modify()->clear();
			_showMore->setHidden(true);
		}

		_entriesContainer->clear();
		updateInfo();
	}));

	_showMore->clicked().connect(std::bind([=]
	{
		addSome();
	}));

	updateInfo();
	addSome();
}

void
PlayQueue::stop()
{
	updateCurrentTrack(false);
	_trackPos.reset();
	playbackStop.emit();
}

void
PlayQueue::play(std::size_t pos)
{
	updateCurrentTrack(false);

	Database::IdType trackId;
	{
		Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

		auto playlist = Database::Playlist::get(LmsApp->getDboSession(), currentPlayQueueName, LmsApp->getCurrentUser());

		// If out of range, stop playing
		if (pos >= playlist->getCount())
		{
			stop();
			return;
		}

		// If last and radio mode, fill the next song
		if (_radioMode->checkState() == Wt::CheckState::Checked && pos == playlist->getCount() - 1)
			addRadioTrack();

		_trackPos = pos;
		trackId = playlist->getEntry(*_trackPos)->getTrack().id();
		updateCurrentTrack(true);
	}

	playTrack.emit(trackId);
}

void
PlayQueue::playPrevious()
{
	if (!_trackPos)
		return;

	if (*_trackPos == 0)
		stop();
	else
		play(*_trackPos - 1);
}

void
PlayQueue::playNext()
{
	if (!_trackPos)
	{
		play(0);
		return;
	}

	play(*_trackPos + 1);
}

void
PlayQueue::updateInfo()
{
	Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

	auto playlist = Database::Playlist::get(LmsApp->getDboSession(), currentPlayQueueName, LmsApp->getCurrentUser());
	_nbTracks->setText(Wt::WString::tr("Lms.PlayQueue.nb-tracks").arg(static_cast<unsigned>(playlist->getCount())));
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

	LMS_LOG(UI, DEBUG) << "Adding tracks to the current queue";

	auto playlist = Database::Playlist::get(LmsApp->getDboSession(), currentPlayQueueName, LmsApp->getCurrentUser());

	for (auto track : tracks)
		Database::PlaylistEntry::create(LmsApp->getDboSession(), track, playlist);

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
	LmsApp->notifyMsg(Wt::WString::tr("Lms.PlayQueue.nb-tracks-added").arg(tracks.size()));
}

void
PlayQueue::playTracks(const std::vector<Database::Track::pointer>& tracks)
{
	LMS_LOG(UI, DEBUG) << "Emptying current queue to play new tracks";

	Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

	auto playqueue = Database::Playlist::get(LmsApp->getDboSession(), currentPlayQueueName, LmsApp->getCurrentUser());
	playqueue.modify()->clear();

	_entriesContainer->clear();

	enqueueTracks(tracks);
	play(0);

	LmsApp->notifyMsg(Wt::WString::tr("Lms.PlayQueue.nb-tracks-playing").arg(tracks.size()));
}


void
PlayQueue::addSome()
{
	Wt::Dbo::Transaction transaction (LmsApp->getDboSession());

	auto playlist = Database::Playlist::get(LmsApp->getDboSession(), currentPlayQueueName, LmsApp->getCurrentUser());

	bool moreResults;
	auto playlistEntries = playlist->getEntries(_entriesContainer->count(), 50, moreResults);
	for (auto playlistEntry : playlistEntries)
	{
		auto playlistEntryId = playlistEntry.id();
		auto track = playlistEntry->getTrack();

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
				play(pos);
		}));

		Wt::WText* delBtn = entry->bindNew<Wt::WText>("del-btn", Wt::WString::tr("Lms.PlayQueue.delete"), Wt::TextFormat::XHTML);
		delBtn->clicked().connect(std::bind([=]
		{
			// Remove the entry n both the widget tree and the playqueue
			{
				Wt::Dbo::Transaction transaction (LmsApp->getDboSession());

				auto entryToRemove = Database::PlaylistEntry::getById(LmsApp->getDboSession(), playlistEntryId);
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

	_showMore->setHidden(!moreResults);
}

void
PlayQueue::addRadioTrack()
{
	auto now = std::chrono::system_clock::now();
	std::mt19937 randGenerator(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());

	LMS_LOG(UI, INFO) << "Radio mode: adding track";

	auto playlist = Database::Playlist::get(LmsApp->getDboSession(), currentPlayQueueName, LmsApp->getCurrentUser());

	// Get all the tracks of the playlist, get the cluster that is mostly used
	// and reuse it to get the next track
	auto clusters = playlist->getClusters();
	if (clusters.empty())
		return;

	for (auto cluster : clusters)
	{
		LMS_LOG(UI, DEBUG) << "Processing cluster '" << cluster->getName() << "'";

		auto nbTracks = cluster->getTrackCount();
		if (nbTracks == 0)
			continue;

		std::uniform_int_distribution<int> dist(0, nbTracks - 1);

		auto trackToAdd = cluster->getTracks(dist(randGenerator), 1).front();

		if (!playlist->hasTrack(trackToAdd.id()))
		{
			enqueueTrack(trackToAdd);
			return;
		}
	}

	LMS_LOG(UI, INFO) << "No more track to be added!";
}

} // namespace UserInterface

