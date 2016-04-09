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
#include <Wt/WMessageBox>

#include <Wt/WLengthValidator>
#include <Wt/WDialog>
#include <Wt/WLineEdit>
#include <Wt/WLabel>

#include "logger/Logger.hpp"
#include "LmsApplication.hpp"

#include "TableFilter.hpp"
#include "KeywordSearchFilter.hpp"

#include "DesktopAudio.hpp"

namespace {

void WPopupMenuClear(Wt::WPopupMenu* menu)
{
	while(menu->count() > 0)
		menu->removeItem(menu->itemAt(0));
}

}

namespace UserInterface {
namespace Desktop {

using namespace Database;

// Special playlist generated each time the playque gets changed
// Restored at the beginning of the session
static const std::string CurrentQueuePlaylistName = "__current__";

Audio::Audio(Wt::WContainerWidget* parent)
: UserInterface::Audio(parent),
_mediaPlayer(nullptr),
_trackView(nullptr),
_playQueue(nullptr)
{
	Wt::WGridLayout *mainLayout = new Wt::WGridLayout();
	this->setLayout(mainLayout);
	mainLayout->setContentsMargins(9,4,9,9);

	// Filters
	Wt::WHBoxLayout *filterLayout = new Wt::WHBoxLayout();

	TableFilterGenre *filterGenre = new TableFilterGenre();
	filterLayout->addWidget(filterGenre);
	_filterChain.addFilter(filterGenre);

	TableFilterArtist *filterArtist = new TableFilterArtist();
	filterLayout->addWidget(filterArtist);
	_filterChain.addFilter(filterArtist);

	TableFilterRelease *filterRelease = new TableFilterRelease();
	filterLayout->addWidget(filterRelease);
	_filterChain.addFilter(filterRelease);

	mainLayout->addLayout(filterLayout, 0, 1);
	// ENDOF(Filters)

	// TODO ADD some stats (nb files, total duration, etc.)

	Wt::WVBoxLayout* trackLayout = new Wt::WVBoxLayout();

	_trackView = new TrackView();
	trackLayout->addWidget(_trackView, 1);

	Wt::WHBoxLayout* trackControls = new Wt::WHBoxLayout();

	Wt::WPushButton* playBtn = new Wt::WPushButton("Play");
	playBtn->setStyleClass("btn-sm");
	trackControls->addWidget(playBtn);

	Wt::WPushButton* addBtn = new Wt::WPushButton("Add");
	addBtn->setStyleClass("btn-sm");
	trackControls->addWidget(addBtn);

	Wt::WText *statsText = new Wt::WText();
	statsText->setStyleClass("vertical-align");
	trackControls->addWidget(statsText, 1);
	_trackView->statsUpdated().connect(std::bind([=] (Wt::WString stats) {
		statsText->setText(stats);
	}, std::placeholders::_1));

	trackLayout->addLayout(trackControls);

	mainLayout->addLayout(trackLayout, 1, 1);

	_filterChain.addFilter(_trackView);


	_playQueue = new PlayQueue();

	// Playlist/PlayQueue
	Wt::WContainerWidget* playQueueContainer = new Wt::WContainerWidget();
	playQueueContainer->setStyleClass("playqueue");
	Wt::WVBoxLayout* playQueueLayout = new Wt::WVBoxLayout();
	playQueueContainer->setLayout(playQueueLayout);

	playQueueLayout->addWidget( _playQueue, 1);

	Wt::WHBoxLayout* playlistControls = new Wt::WHBoxLayout();

	Wt::WPushButton *playlistBtn = new Wt::WPushButton("Playlist");
	playlistBtn->setStyleClass("btn-sm btn-primary");
	playlistControls->addWidget(playlistBtn);

	// Playlist menu
	{
		Wt::WPopupMenu *popupMain = new Wt::WPopupMenu();

		_popupMenuSave = new Wt::WPopupMenu();
		popupMain->addMenu("Save", _popupMenuSave);

		_popupMenuLoad = new Wt::WPopupMenu();
		popupMain->addMenu("Load", _popupMenuLoad);

		_popupMenuDelete = new Wt::WPopupMenu();
		popupMain->addMenu("Delete", _popupMenuDelete);

		playlistBtn->setMenu(popupMain);
	}

	Wt::WPushButton *upBtn = new Wt::WPushButton("<i class =\"fa fa-arrow-up fa-lg\"></i>", Wt::XHTMLText);
	upBtn->setStyleClass("btn-sm");
	playlistControls->addWidget(upBtn);
	Wt::WPushButton *downBtn = new Wt::WPushButton("<i class =\"fa fa-arrow-down fa-lg\"></i>", Wt::XHTMLText);
	downBtn->setStyleClass("btn-sm");
	playlistControls->addWidget(downBtn);
	Wt::WPushButton *delBtn = new Wt::WPushButton("<i class =\"fa fa-remove fa-lg\"></i>", Wt::XHTMLText);
	delBtn->setStyleClass("btn-sm btn-warning");
	playlistControls->addWidget(delBtn);
	Wt::WPushButton *clearBtn = new Wt::WPushButton("<i class =\"fa fa-trash fa-lg\"></i>", Wt::XHTMLText);
	clearBtn->setStyleClass("btn-sm btn-danger");
	playlistControls->addWidget(clearBtn);

	delBtn->clicked().connect(_playQueue, &PlayQueue::delSelected);
	upBtn->clicked().connect(_playQueue, &PlayQueue::moveSelectedUp);
	downBtn->clicked().connect(_playQueue, &PlayQueue::moveSelectedDown);
	clearBtn->clicked().connect(std::bind([=]
	{
		Wt::WMessageBox *messageBox = new Wt::WMessageBox
			("Clear playqueue",
			 Wt::WString( "Are you sure?"),
			 Wt::Question, Wt::Yes | Wt::No);

		messageBox->setModal(true);

		messageBox->buttonClicked().connect(std::bind([=] ()
		{
			if (messageBox->buttonResult() == Wt::Yes)
				_playQueue->delAll();

			delete messageBox;
		}));

		messageBox->show();
	}));

	playQueueLayout->addLayout(playlistControls);

	mainLayout->addWidget(playQueueContainer, 0, 0, 2, 1);

	// Load the last known queue
	playlistLoadToPlayqueue(CurrentQueuePlaylistName);

	// Select the last known playing track
	{
		Wt::Dbo::Transaction transaction(DboSession());
		_playQueue->select(CurrentUser()->getCurPlayingTrackPos());
	}

	_mediaPlayer = new AudioPlayer(AudioPlayer::ControlShuffle | AudioPlayer::ControlRepeat);
	mainLayout->addWidget(_mediaPlayer, 2, 0, 1, 4);

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

	_playQueue->tracksUpdated().connect(std::bind([=] () {
		LMS_LOG(UI, INFO) << "Playqueue updated!";

		playlistSaveFromPlayqueue(CurrentQueuePlaylistName);
	}));


	playlistRefreshMenus();

	// Initially, search for everything
	_filterChain.searchKeyword("");
}

void
Audio::playlistShowSaveNewDialog()
{
	Wt::WDialog *dialog = new Wt::WDialog("New playlist");

	Wt::WLabel *label = new Wt::WLabel("Name", dialog->contents());
	Wt::WLineEdit *edit = new Wt::WLineEdit(dialog->contents());
	label->setBuddy(edit);

	Wt::WLengthValidator* validator = new Wt::WLengthValidator();
	validator->setMinimumLength(3);
	validator->setMandatory(true);
	edit->setValidator(validator);

	Wt::WPushButton *save = new Wt::WPushButton("Save", dialog->footer());
	save->setStyleClass("btn-success");
	save->setDefault(true);
	save->disable();

	Wt::WPushButton *cancel = new Wt::WPushButton("Cancel", dialog->footer());
	dialog->rejectWhenEscapePressed();

	edit->keyWentUp().connect(std::bind([=] () {
		save->setDisabled(edit->validate() != Wt::WValidator::Valid);
	}));

	save->clicked().connect(std::bind([=] ()
	{
		if (edit->validate())
			dialog->accept();
        }));

	cancel->clicked().connect(dialog, &Wt::WDialog::reject);

	dialog->finished().connect(std::bind([=] ()
	{
		if (dialog->result() == Wt::WDialog::Accepted)
		{
			playlistShowSaveDialog(edit->text().toUTF8());
		}

		delete dialog;
	}));

	dialog->show();
}

void
Audio::playlistShowSaveDialog(std::string playlistName)
{
	Wt::Dbo::Transaction transaction(DboSession());

	// Actually create the dialog only if the given list already exists
	if (Playlist::get(DboSession(), playlistName, CurrentUser()))
	{
		Wt::WMessageBox *messageBox = new Wt::WMessageBox
			("Overwrite playlist",
			 Wt::WString( "Overwrite playlist '{1}'?").arg(playlistName),
			 Wt::Question, Wt::Yes | Wt::No);

		messageBox->setModal(true);

		messageBox->buttonClicked().connect(std::bind([=] () {
			if (messageBox->buttonResult() == Wt::Yes)
				playlistSaveFromPlayqueue(playlistName);

			delete messageBox;
		}));

		messageBox->show();
	}
	else
	{
		playlistSaveFromPlayqueue(playlistName);
		playlistRefreshMenus();
	}
}

void
Audio::playlistSaveFromPlayqueue(std::string playlistName)
{
	LMS_LOG(UI, INFO) << "Saving playqueue to playlist '" << playlistName << "'";

	Wt::Dbo::Transaction transaction(DboSession());

	Playlist::pointer playlist = Playlist::get(DboSession(), playlistName, CurrentUser());
	if (playlist)
	{
		LMS_LOG(UI, INFO) << "Erasing playlist '" << playlistName << "'";
		playlist.remove();
	}

	playlist = Playlist::create(DboSession(), playlistName, false, CurrentUser());

	std::vector<Track::id_type> trackIds;
	_playQueue->getTracks(trackIds);

	int pos = 0;
	for (Track::id_type trackId : trackIds)
	{
		Track::pointer track = Track::getById(DboSession(), trackId);

		if (track)
			PlaylistEntry::create(DboSession(), track, playlist, pos++);
	}

	LMS_LOG(UI, INFO) << "Saving playqueue to playlist '" << playlistName << "' done. Contains " << pos << " entries";
}

void
Audio::playlistLoadToPlayqueue(std::string playlistName)
{
	LMS_LOG(UI, DEBUG) << "Loading playlist '" << playlistName << "' to playqueue";

	std::vector<Track::id_type> entries;

	{
		Wt::Dbo::Transaction transaction(DboSession());

		Playlist::pointer playlist = Playlist::get(DboSession(), playlistName, CurrentUser());
		if (!playlist)
			return;

		entries = PlaylistEntry::getEntries(DboSession(), playlist);
	}

	_playQueue->clear();
	_playQueue->addTracks(entries);

	LMS_LOG(UI, DEBUG) << "Loading playlist '" << playlistName << "' to playqueue done. " << entries.size() << " entries";
}


void
Audio::playlistShowDeleteDialog(std::string name)
{

	Wt::WMessageBox *messageBox = new Wt::WMessageBox
		("Delete playlist",
		 Wt::WString( "Deleting playlist '{1}'?").arg(name),
		 Wt::Question, Wt::Yes | Wt::No);

	messageBox->setModal(true);

	messageBox->buttonClicked().connect(std::bind([=] () {
		if (messageBox->buttonResult() == Wt::Yes)
		{
			Wt::Dbo::Transaction transaction(DboSession());

			Playlist::pointer playlist = Playlist::get(DboSession(), name, CurrentUser());
			if (playlist)
				playlist.remove();

			playlistRefreshMenus();
		}

		delete messageBox;
	}));

	messageBox->show();
}

void
Audio::playlistRefreshMenus()
{
	Wt::Dbo::Transaction transaction(DboSession());

	// Clear playlists in each menu
	LMS_LOG(UI, DEBUG) << "Save item count: " << _popupMenuSave->count();

	WPopupMenuClear(_popupMenuDelete);
	WPopupMenuClear(_popupMenuLoad);
	WPopupMenuClear(_popupMenuSave);

	_popupMenuSave->addItem("New")->triggered().connect(std::bind([=] ()
	{
		playlistShowSaveNewDialog();
	}));
	_popupMenuSave->addSeparator();

	std::vector<Playlist::pointer> playlists = Playlist::get(DboSession(), CurrentUser());

	for (Playlist::pointer playlist : playlists)
	{
		if (playlist->getName() == CurrentQueuePlaylistName)
			continue;

		// Add playlists in each menu
		_popupMenuDelete->addItem(playlist->getName())->triggered().connect(std::bind([=] ()
		{
			playlistShowDeleteDialog(playlist->getName());
		}));

		_popupMenuLoad->addItem(playlist->getName())->triggered().connect(std::bind([=] ()
		{
			playlistLoadToPlayqueue(playlist->getName());
			playlistRefreshMenus(); // in case deleted in other session
		}));

		_popupMenuSave->addItem(playlist->getName())->triggered().connect(std::bind([=] ()
		{
			playlistShowSaveDialog(playlist->getName());
		}));
	}
}


void
Audio::search(std::string searchText)
{
	_filterChain.searchKeyword(searchText);
}

void
Audio::addSelectedTracks(void)
{
	std::vector<Track::id_type> trackIds;
	_trackView->getSelectedTracks(trackIds);

	// If nothing is selected, get the whole track list
	if (trackIds.empty())
		_trackView->getTracks(trackIds);

	_playQueue->addTracks(trackIds);
}

void
Audio::playSelectedTracks(PlayQueueAddType addType)
{
	std::vector<Track::id_type> trackIds;

	LMS_LOG(UI, DEBUG) << "Playing selected tracks... nb selected = " << _trackView->getNbSelectedTracks() << ", add type = " << (addType == PlayQueueAddAllTracks ? "AddAll" : "AddSelected");

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

			LMS_LOG(UI, DEBUG) << "Adding selected tracks...";

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
Audio::playTrack(Track::id_type trackId, int pos)
{
	{
		Wt::Dbo::Transaction transaction(DboSession());

		// Update user current track position
		CurrentUser().modify()->setCurPlayingTrackPos(pos);
	}

	if (!_mediaPlayer->loadTrack(trackId))
		_playQueue->playNext();
}

} // namespace Desktop
} // namespace UserInterface

