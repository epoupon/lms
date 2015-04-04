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

// Special playlist generated each time the playque gets changed
// Restored at the beginning of the session
static const std::string CurrentQueuePlaylistName = "__current__";

Audio::Audio(SessionData& sessionData, Wt::WContainerWidget* parent)
: UserInterface::Audio(parent),
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

		// Determine the encoding to be used
		Wt::WMediaPlayer::Encoding encoding;
		switch (user->getAudioEncoding())
		{
			case Database::AudioEncoding::MP3: encoding = Wt::WMediaPlayer::MP3; break;
			case Database::AudioEncoding::WEBMA: encoding = Wt::WMediaPlayer::WEBMA; break;
			case Database::AudioEncoding::OGA: encoding = Wt::WMediaPlayer::OGA; break;
			case Database::AudioEncoding::FLA: encoding = Wt::WMediaPlayer::FLA; break;
			case Database::AudioEncoding::AUTO:
			default:
				encoding = AudioMediaPlayer::getBestEncoding();
		}

		LMS_LOG(MOD_UI, SEV_INFO) << "Audio player using encoding " << encoding;
		_mediaPlayer = new AudioMediaPlayer(encoding);
		playQueueLayout->addWidget(_mediaPlayer);

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


		playQueueLayout->addLayout(playlistControls);

		mainLayout->addWidget(playQueueContainer, 0, 0, 2, 1);

		// Load the last known queue
		playlistLoadToPlayqueue(CurrentQueuePlaylistName);
		// Select the last known playing track
		_playQueue->select(user->getCurPlayingTrackPos());
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

	_playQueue->tracksUpdated().connect(std::bind([=] () {
		LMS_LOG(MOD_UI, SEV_INFO) << "Playqueue updated!";

		playlistSaveFromPlayqueue(CurrentQueuePlaylistName);
	}));


	playlistRefreshMenus();
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
	Wt::Dbo::Transaction transaction(_db.getSession());

	Database::User::pointer user = _db.getCurrentUser();
	if (!user)
		return;

	// Actually create the dialog only if the given list already exists
	if (Database::Playlist::get(_db.getSession(), playlistName, user))
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
	LMS_LOG(MOD_UI, SEV_INFO) << "Saving playqueue to playlist '" << playlistName << "'";

	Wt::Dbo::Transaction transaction(_db.getSession());

	Database::User::pointer user = _db.getCurrentUser();
	if (!user)
		return;

	Database::Playlist::pointer playlist = Database::Playlist::get(_db.getSession(), playlistName, user);
	if (playlist)
	{
		LMS_LOG(MOD_UI, SEV_INFO) << "Erasing playlist '" << playlistName << "'";
		playlist.remove();
	}

	playlist = Database::Playlist::create(_db.getSession(), playlistName, false, user);

	std::vector<Database::Track::id_type> trackIds;
	_playQueue->getTracks(trackIds);

	int pos = 0;
	BOOST_FOREACH(Database::Track::id_type trackId, trackIds)
	{
		Database::Track::pointer track = Database::Track::getById(_db.getSession(), trackId);

		if (track)
			Database::PlaylistEntry::create(_db.getSession(), track, playlist, pos++);
	}

	LMS_LOG(MOD_UI, SEV_INFO) << "Saving playqueue to playlist '" << playlistName << "' done. Contains " << pos << " entries";
}

void
Audio::playlistLoadToPlayqueue(std::string playlistName)
{
	LMS_LOG(MOD_UI, SEV_DEBUG) << "Loading playlist '" << playlistName << "' to playqueue";

	std::vector<Database::Track::id_type> entries;

	{
		Wt::Dbo::Transaction transaction(_db.getSession());

		Database::User::pointer user = _db.getCurrentUser();
		if (!user)
			return;

		Database::Playlist::pointer playlist = Database::Playlist::get(_db.getSession(), playlistName, user);
		if (!playlist)
			return;

		entries = Database::PlaylistEntry::getEntries(_db.getSession(), playlist);
	}

	_playQueue->clear();
	_playQueue->addTracks(entries);

	LMS_LOG(MOD_UI, SEV_DEBUG) << "Loading playlist '" << playlistName << "' to playqueue done. " << entries.size() << " entries";
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
			Wt::Dbo::Transaction transaction(_db.getSession());
			Database::User::pointer user = _db.getCurrentUser();
			if (!user)
				return;

			Database::Playlist::pointer playlist = Database::Playlist::get(_db.getSession(), name, user);
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
	Wt::Dbo::Transaction transaction(_db.getSession());

	Database::User::pointer user = _db.getCurrentUser();
	if (!user)
		return;

	// Clear playlists in each menu
	LMS_LOG(MOD_UI, SEV_DEBUG) << "Save item count: " << _popupMenuSave->count();

	WPopupMenuClear(_popupMenuDelete);
	WPopupMenuClear(_popupMenuLoad);
	WPopupMenuClear(_popupMenuSave);

	_popupMenuSave->addItem("New")->triggered().connect(std::bind([=] ()
	{
		playlistShowSaveNewDialog();
	}));
	_popupMenuSave->addSeparator();

	std::vector<Database::Playlist::pointer> playlists = Database::Playlist::get(_db.getSession(), user);

	BOOST_FOREACH(Database::Playlist::pointer playlist, playlists)
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
Audio::playTrack(boost::filesystem::path p, int pos)
{
	LMS_LOG(MOD_UI, SEV_DEBUG) << "play track '" << p << "'";
	try {

		std::size_t bitrate = 0;

		// Get user preferences
		{
			Wt::Dbo::Transaction transaction(_db.getSession());
			Database::User::pointer user = _db.getCurrentUser();
			if (user)
			{
				bitrate = user->getAudioBitrate();
				user.modify()->setCurPlayingTrackPos(pos);
			}
			else
			{
				LMS_LOG(MOD_UI, SEV_ERROR) << "Can't play: user does not exists!";
				return; // TODO logout?
			}
		}

		Transcode::InputMediaFile inputFile(p);

		// Determine the output format using the encoding of the player
		Transcode::Format::Encoding encoding;
		switch (_mediaPlayer->getEncoding())
		{
			case Wt::WMediaPlayer::MP3: encoding = Transcode::Format::MP3; break;
			case Wt::WMediaPlayer::FLA: encoding = Transcode::Format::FLA; break;
			case Wt::WMediaPlayer::OGA: encoding = Transcode::Format::OGA; break;
			case Wt::WMediaPlayer::WEBMA: encoding = Transcode::Format::WEBMA; break;
			default:
			    encoding = Transcode::Format::MP3;
		}

		Transcode::Parameters parameters(inputFile, Transcode::Format::get(encoding));

		parameters.setBitrate(Transcode::Stream::Audio, bitrate);

		_mediaPlayer->load( parameters );
	}
	catch( std::exception &e)
	{
		LMS_LOG(MOD_UI, SEV_ERROR) << "Caught exception while loading '" << p << "': " << e.what();
	}
}

} // namespace Desktop
} // namespace UserInterface

