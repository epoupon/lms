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

#include "logger/Logger.hpp"

#include "TableFilter.hpp"
#include "KeywordSearchFilter.hpp"
#include "TrackView.hpp"

#include "Audio.hpp"

namespace UserInterface {

Audio::Audio(SessionData& sessionData, Wt::WContainerWidget* parent)
: Wt::WContainerWidget(parent),
_db(sessionData.getDatabaseHandler()),
_mediaPlayer(nullptr)
{

	Wt::WGridLayout *mainLayout = new Wt::WGridLayout();
	this->setLayout(mainLayout);

	// Filters
	Wt::WHBoxLayout *filterLayout = new Wt::WHBoxLayout();

	{
		TableFilter *filter = new TableFilter(_db, "genre", "name");
		filterLayout->addWidget(filter);
		_filterChain.addFilter(filter);
	}
	{
		TableFilter *filter = new TableFilter(_db, "artist", "name");
		filterLayout->addWidget(filter);
		_filterChain.addFilter(filter);
	}
	{
		TableFilter *filter = new TableFilter(_db, "release", "name");
		filterLayout->addWidget(filter);
		_filterChain.addFilter(filter);
	}

	mainLayout->addLayout(filterLayout, 0, 0);
	// ENDOF(Filters)

	TrackView* trackView = new TrackView(_db);
	mainLayout->addWidget( trackView, 1, 0);
	_filterChain.addFilter(trackView);

	// TODO Playlist here
	{
		Wt::WVBoxLayout* playlist = new Wt::WVBoxLayout();

		Wt::WHBoxLayout* playlistControls = new Wt::WHBoxLayout();

		Wt::WComboBox *cb = new Wt::WComboBox();
		cb->addItem("metal");
		cb->addItem("rock");
		cb->addItem("top50");
		cb->setWidth(75);

		playlistControls->addWidget(cb, 1);
		playlistControls->addWidget(new Wt::WPushButton("Rename"));
		playlistControls->addWidget(new Wt::WPushButton("+"));
		playlistControls->addWidget(new Wt::WPushButton("-"));

		playlist->addLayout(playlistControls);

		// TODO
		playlist->addWidget( new Wt::WTableView(), 1);

		mainLayout->addLayout(playlist, 0, 1, 2, 1);
	}

	_mediaPlayer = new AudioMediaPlayer();
	mainLayout->addWidget(_mediaPlayer, 2, 0, 1, 2);

	mainLayout->setRowStretch(1, 1);
	mainLayout->setRowResizable(0, true, Wt::WLength(200, Wt::WLength::Pixel));
	mainLayout->setColumnResizable(0, true);

	trackView->trackSelected().connect(boost::bind(&Audio::playTrack, this, _1));

	_mediaPlayer->playbackEnded().connect(std::bind([=] ()
	{
		// TODO link to playlist
		trackView->selectNextTrack();
	}));
}

void
Audio::search(const std::string& searchText)
{
	_filterChain.searchKeyword(searchText);
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

