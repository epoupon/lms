/*
 * Copyright (C) 2015 Emeric Poupon
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


#include <Wt/WContainerWidget>
#include <Wt/WTemplate>
#include <Wt/WText>

#include "logger/Logger.hpp"
#include "utils/Utils.hpp"

#include "audio/AudioPlayer.hpp"
#include "LmsApplication.hpp"

#include "MobileAudio.hpp"

#define SEARCH_NB_ITEMS	4

namespace UserInterface {
namespace Mobile {

using namespace Database;

static void playTrack(AudioPlayer *audioPlayer, Database::Track::id_type trackId)
{
	LMS_LOG(UI, DEBUG) << "Playing track id " << trackId;

	Wt::Dbo::Transaction transaction(DboSession());

	Track::pointer track = Track::getById(DboSession(), trackId);

	if (track)
		audioPlayer->loadTrack(track.id());
}

void
Audio::search(std::string text)
{
	// When a new search is done, output some results from:
	// Artist
	// Release
	// Song
	auto keywords = splitString(text, " ");;

	_releaseSearch->search(SearchFilter::ByNameAnd(SearchFilter::Field::Release, keywords), SEARCH_NB_ITEMS);
	_artistSearch->search(SearchFilter::ByNameAnd(SearchFilter::Field::Artist, keywords), SEARCH_NB_ITEMS);
	_trackSearch->search(SearchFilter::ByNameAnd(SearchFilter::Field::Track, keywords), SEARCH_NB_ITEMS);

	_artistSearch->show();
	_releaseSearch->show();
	_trackSearch->show();
	_trackReleaseView->hide();
}


Audio::Audio(Wt::WContainerWidget *parent)
: UserInterface::Audio(parent)
{
	// Root div has to be a "container"
	this->setStyleClass("container-fluid");
	this->setPadding(60, Wt::Bottom);

	_artistSearch = new ArtistSearch(this);
	_releaseSearch = new ReleaseSearch(this);
	_trackSearch = new TrackSearch(this);

	_trackReleaseView  = new TrackReleaseView(this);
	_trackReleaseView->hide();

	Wt::WTemplate* footer = new Wt::WTemplate(this);
	footer->setTemplateText(Wt::WString::tr("mobile-audio-footer"));

	AudioPlayer* audioPlayer = new AudioPlayer();
	footer->bindWidget("player", audioPlayer);

	_artistSearch->moreArtistsSelected().connect(std::bind([=]
	{
		_releaseSearch->hide();
		_trackSearch->hide();
		_artistSearch->show();
		_trackReleaseView->hide();
	}));

	_artistSearch->artistSelected().connect(std::bind([=] (Artist::id_type artistId)
	{
		_artistSearch->hide();
		_trackSearch->hide();
		_releaseSearch->show();
		_trackReleaseView->hide();

		_releaseSearch->search(SearchFilter::ById(SearchFilter::Field::Artist, artistId), 20);
	}, std::placeholders::_1));

	_releaseSearch->moreReleasesSelected().connect(std::bind([=]
	{
		_artistSearch->hide();
		_trackSearch->hide();
		_releaseSearch->show();
		_trackReleaseView->hide();
	}));

	_releaseSearch->releaseSelected().connect(std::bind([=] (Release::id_type releaseId)
	{
		_artistSearch->hide();
		_releaseSearch->hide();
		_trackSearch->hide();
		_trackReleaseView->show();

		_trackReleaseView->search(SearchFilter::ById(SearchFilter::Field::Release, releaseId), 40);
	}, std::placeholders::_1));

	_trackSearch->moreSelected().connect(std::bind([=]
	{
		_artistSearch->hide();
		_releaseSearch->hide();
		_trackSearch->show();
		_trackReleaseView->hide();
	}));

	_trackSearch->trackPlay().connect(std::bind([=] (Track::id_type id)
	{
		playTrack(audioPlayer, id);
	}, std::placeholders::_1));

	_trackReleaseView->trackPlay().connect(std::bind([=] (Track::id_type id)
	{
		playTrack(audioPlayer, id);
	}, std::placeholders::_1));

	// Initially, populate the widgets using an empty search
	{
		_artistSearch->search(SearchFilter(), SEARCH_NB_ITEMS);
		_releaseSearch->search(SearchFilter(), SEARCH_NB_ITEMS);
		_trackSearch->search(SearchFilter(), SEARCH_NB_ITEMS);
	}

}

} // namespace Mobile
} // namespace UserInterface

