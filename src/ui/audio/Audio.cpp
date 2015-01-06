/*
 * Copyright (C) 2013 Emeric Poupon
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

#include <boost/bind.hpp>

#include <Wt/WVBoxLayout>
#include <Wt/WHBoxLayout>
#include <Wt/WGridLayout>
#include <Wt/WComboBox>
#include <Wt/WPushButton>
#include <Wt/WPopupMenu>

#include "logger/Logger.hpp"

#include "TableFilter.hpp"
#include "KeywordSearchFilter.hpp"

#include "Audio.hpp"

namespace UserInterface {

Audio::Audio(SessionData& sessionData, Wt::WContainerWidget* parent)
: Wt::WContainerWidget(parent),
_db(sessionData.getDatabaseHandler()),
_mediaPlayer(nullptr),
_trackView(nullptr),
_playQueue(nullptr)
{
	Wt::WGridLayout *mainLayout = new Wt::WGridLayout();
	this->setLayout(mainLayout);
	mainLayout->setContentsMargins(9,4,9,9);

	// Filters
	Wt::WHBoxLayout *filterLayout = new Wt::WHBoxLayout();

	TableFilterGenre *filterGenre = new TableFilterGenre(_db);
	filterLayout->addWidget(filterGenre);
	_filterChain.addFilter(filterGenre);

	TableFilterArtist *filterArtist = new TableFilterArtist(_db);
	filterLayout->addWidget(filterArtist);
	_filterChain.addFilter(filterArtist);

	TableFilterRelease *filterRelease = new TableFilterRelease(_db);
	filterLayout->addWidget(filterRelease);
	_filterChain.addFilter(filterRelease);

	mainLayout->addLayout(filterLayout, 0, 1);
	// ENDOF(Filters)

	// TODO ADD some stats (nb files, total duration, etc.)

	Wt::WVBoxLayout* trackLayout = new Wt::WVBoxLayout();

	_trackView = new TrackView(_db);
	trackLayout->addWidget(_trackView, 1);

	Wt::WHBoxLayout* trackControls = new Wt::WHBoxLayout();

	Wt::WPushButton* playBtn = new Wt::WPushButton("Play");
	playBtn->setStyleClass("btn-sm");
	trackControls->addWidget(playBtn);

	Wt::WPushButton* addBtn = new Wt::WPushButton("Add");
	addBtn->setStyleClass("btn-sm");
	trackControls->addWidget(addBtn);
	trackControls->addWidget(new Wt::WText("Total duration: "), 1);


	trackLayout->addLayout(trackControls);

	mainLayout->addLayout(trackLayout, 1, 1);

	_filterChain.addFilter(_trackView);

	_playQueue = new PlayQueue(_db);

	// Playlist/PlayQueue
	{
		Wt::Dbo::Transaction transaction(_db.getSession());
		Database::User::pointer user = _db.getCurrentUser();

		Wt::WContainerWidget* playQueueContainer = new Wt::WContainerWidget();
		playQueueContainer->setStyleClass("playqueue");
		Wt::WVBoxLayout* playQueueLayout = new Wt::WVBoxLayout();
		playQueueContainer->setLayout(playQueueLayout);

		_mediaPlayer = new AudioMediaPlayer();
		playQueueLayout->addWidget(_mediaPlayer);

		playQueueLayout->addWidget( _playQueue, 1);

		Wt::WHBoxLayout* playlistControls = new Wt::WHBoxLayout();

		Wt::WPushButton *saveBtn = new Wt::WPushButton("Save");
		saveBtn->setStyleClass("btn-sm");

		playlistControls->addWidget(saveBtn);
		Wt::WPushButton *loadBtn = new Wt::WPushButton("Load");
		loadBtn->setStyleClass("btn-sm");
		playlistControls->addWidget(loadBtn);

		Wt::WPushButton *upBtn = new Wt::WPushButton("UP");
		upBtn->setStyleClass("btn-sm");
		playlistControls->addWidget(upBtn);
		Wt::WPushButton *downBtn = new Wt::WPushButton("DO");
		downBtn->setStyleClass("btn-sm");
		playlistControls->addWidget(downBtn);
		Wt::WPushButton *delBtn = new Wt::WPushButton("DEL");
		delBtn->setStyleClass("btn-sm btn-warning");
		playlistControls->addWidget(delBtn);
		Wt::WPushButton *clearBtn = new Wt::WPushButton("CLR");
		clearBtn->setStyleClass("btn-sm btn-danger");
		playlistControls->addWidget(clearBtn);

		delBtn->clicked().connect(_playQueue, &PlayQueue::delSelected);
		upBtn->clicked().connect(_playQueue, &PlayQueue::moveSelectedUp);
		downBtn->clicked().connect(_playQueue, &PlayQueue::moveSelectedDown);
		clearBtn->clicked().connect(_playQueue, &PlayQueue::delAll);

		// Load menu
		{
			Wt::WPopupMenu *popup = new Wt::WPopupMenu();

			popup->addItem("Metal");
			popup->addItem("Test");

			loadBtn->setMenu(popup);
		}

		// Save Menu
		{
			Wt::WPopupMenu *popup = new Wt::WPopupMenu();

			popup->addItem("New");
			popup->addSeparator();
			popup->addItem("Metal");
			popup->addItem("Test");

			saveBtn->setMenu(popup);
		}

		playQueueLayout->addLayout(playlistControls);

		mainLayout->addWidget(playQueueContainer, 0, 0, 2, 1);
	}

	mainLayout->setRowStretch(1, 1);
	mainLayout->setRowResizable(0, true, Wt::WLength(250, Wt::WLength::Pixel));
	mainLayout->setColumnResizable(0, true, Wt::WLength(400, Wt::WLength::Pixel));

	// Double click on track
	// Set the selected tracks to the play queue
	_trackView->trackDoubleClicked().connect(boost::bind(&Audio::playSelectedTracks, this, PlayQueueAddSelectedTracks));

	// Double click on artist
	// Set the selected tracks to the play queue
	filterArtist->sigDoubleClicked().connect(boost::bind(&Audio::playSelectedTracks, this, PlayQueueAddAllTracks));
	filterRelease->sigDoubleClicked().connect(boost::bind(&Audio::playSelectedTracks, this, PlayQueueAddAllTracks));
	filterGenre->sigDoubleClicked().connect(boost::bind(&Audio::playSelectedTracks, this, PlayQueueAddAllTracks));

	// Play button
	// Set the selected tracks to the play queue
	playBtn->clicked().connect(boost::bind(&Audio::playSelectedTracks, this, PlayQueueAddSelectedTracks));

	// Add Button
	// Add the selected tracks at the end of the play queue
	addBtn->clicked().connect(this, &Audio::addSelectedTracks);

	_playQueue->playTrack().connect(this, &Audio::playTrack);

	_mediaPlayer->playbackEnded().connect(_playQueue, &PlayQueue::handlePlaybackComplete);
	_mediaPlayer->playNext().connect(_playQueue, &PlayQueue::playNext);
	_mediaPlayer->playPrevious().connect(_playQueue, &PlayQueue::playPrevious);
	_mediaPlayer->shuffle().connect(boost::bind(&PlayQueue::setShuffle, _playQueue, _1));
	_mediaPlayer->loop().connect(boost::bind(&PlayQueue::setLoop,_playQueue, _1));
}

void
Audio::search(const std::string& searchText)
{
	_filterChain.searchKeyword(searchText);
}

void
Audio::addSelectedTracks(void)
{
	std::vector<Database::Track::id_type> trackIds;
	_trackView->getSelectedTracks(trackIds);

	// If nothing is selected, get the whole track list
	if (trackIds.empty())
		_trackView->getTracks(trackIds);

	_playQueue->addTracks(trackIds);
}

void
Audio::playSelectedTracks(PlayQueueAddType addType)
{
	std::vector<Database::Track::id_type> trackIds;

	_playQueue->clear();

	switch(addType)
	{
		case PlayQueueAddAllTracks:
			// Play all the tracks
			_trackView->getTracks(trackIds);
			_playQueue->addTracks(trackIds);
			_playQueue->play();

			break;
		case PlayQueueAddSelectedTracks:
			// If nothing selected, get all the track and play everything
			if (_trackView->getNbSelectedTracks() == 0)
			{
				_trackView->getTracks(trackIds);
				_playQueue->addTracks(trackIds);
				_playQueue->play();
			}
			// If only ONE selected, get them all and pre select the track
			else if (_trackView->getNbSelectedTracks() == 1)
			{
				// Start to play at the selected track, if any
				_trackView->getTracks(trackIds);
				_playQueue->addTracks(trackIds);
				_playQueue->play(_trackView->getFirstSelectedTrackPosition());
			}
			else
			{
				// Play all the selected tracks
				_trackView->getSelectedTracks(trackIds);
				_playQueue->addTracks(trackIds);
				_playQueue->play();
			}
			break;
	}
}

void
Audio::playTrack(boost::filesystem::path p)
{
	LMS_LOG(MOD_UI, SEV_DEBUG) << "play track '" << p << "'";
	try {

		std::size_t bitrate = 0;

		// Get user preferences
		{
			Wt::Dbo::Transaction transaction(_db.getSession());
			Database::User::pointer user = _db.getCurrentUser();
			if (user)
				bitrate = user->getAudioBitrate();
			else
			{
				LMS_LOG(MOD_UI, SEV_ERROR) << "Can't play: user does not exists!";
				return; // TODO logout?
			}
		}

		Transcode::InputMediaFile inputFile(p);

		Transcode::Parameters parameters(inputFile, Transcode::Format::get(Transcode::Format::OGA));

		parameters.setBitrate(Transcode::Stream::Audio, bitrate);

		_mediaPlayer->load( parameters );
	}
	catch( std::exception &e)
	{
		LMS_LOG(MOD_UI, SEV_ERROR) << "Caught exception while loading '" << p << "': " << e.what();
	}
}


} // namespace UserInterface

