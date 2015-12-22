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
#include <Wt/WLineEdit>
#include <Wt/WText>
#include <Wt/WTable>

#include "logger/Logger.hpp"
#include "utils/Utils.hpp"

#include "audio/AudioPlayer.hpp"
#include "LmsApplication.hpp"

#include "ArtistSearch.hpp"
#include "ReleaseSearch.hpp"
#include "TrackSearch.hpp"
#include "TrackReleaseView.hpp"

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

Audio::Audio(Wt::WContainerWidget *parent)
: UserInterface::Audio(parent)
{
	// Root div has to be a "container"
	this->setStyleClass("container-fluid");
	this->setPadding(60, Wt::Bottom);

	Wt::WTemplate* search = new Wt::WTemplate(this);
	search->setTemplateText(Wt::WString::tr("mobile-search"));

	Wt::WLineEdit *edit = new Wt::WLineEdit();
	edit->setEmptyText("Search...");

	search->bindWidget("search", edit);
	search->setMargin(10);

	ArtistSearch* artistSearch = new ArtistSearch(this);
	ReleaseSearch* releaseSearch = new ReleaseSearch(this);
	TrackSearch* trackSearch = new TrackSearch(this);

	TrackReleaseView* trackReleaseView  = new TrackReleaseView(this);
	trackReleaseView->hide();

	Wt::WTemplate* footer = new Wt::WTemplate(this);
	footer->setTemplateText(Wt::WString::tr("mobile-audio-footer"));

	AudioPlayer* audioPlayer = new AudioPlayer();
	footer->bindWidget("player", audioPlayer);

	edit->changed().connect(std::bind([=] ()
	{
		// When a new search is done, output some results from:
		// Artist
		// Release
		// Song
		auto keywords = splitString(edit->text().toUTF8(), " ");;

		releaseSearch->search(SearchFilter::ByNameAnd(SearchFilter::Field::Release, keywords), SEARCH_NB_ITEMS);
		artistSearch->search(SearchFilter::ByNameAnd(SearchFilter::Field::Artist, keywords), SEARCH_NB_ITEMS);
		trackSearch->search(SearchFilter::ByNameAnd(SearchFilter::Field::Track, keywords), SEARCH_NB_ITEMS);

		artistSearch->show();
		releaseSearch->show();
		trackSearch->show();
		trackReleaseView->hide();
	}));

	artistSearch->moreArtistsSelected().connect(std::bind([=]
	{
		releaseSearch->hide();
		trackSearch->hide();
		artistSearch->show();
		trackReleaseView->hide();
	}));

	artistSearch->artistSelected().connect(std::bind([=] (Artist::id_type artistId)
	{
		artistSearch->hide();
		trackSearch->hide();
		releaseSearch->show();
		trackReleaseView->hide();

		releaseSearch->search(SearchFilter::ById(SearchFilter::Field::Artist, artistId), 20);
	}, std::placeholders::_1));

	releaseSearch->moreReleasesSelected().connect(std::bind([=]
	{
		artistSearch->hide();
		trackSearch->hide();
		releaseSearch->show();
		trackReleaseView->hide();
	}));

	releaseSearch->releaseSelected().connect(std::bind([=] (Release::id_type releaseId)
	{
		artistSearch->hide();
		releaseSearch->hide();
		trackSearch->hide();
		trackReleaseView->show();

		trackReleaseView->search(SearchFilter::ById(SearchFilter::Field::Release, releaseId), 40);
	}, std::placeholders::_1));

	trackSearch->moreSelected().connect(std::bind([=]
	{
		artistSearch->hide();
		releaseSearch->hide();
		trackSearch->show();
		trackReleaseView->hide();
	}));

	trackSearch->trackPlay().connect(std::bind([=] (Track::id_type id)
	{
		playTrack(audioPlayer, id);
	}, std::placeholders::_1));

	trackReleaseView->trackPlay().connect(std::bind([=] (Track::id_type id)
	{
		playTrack(audioPlayer, id);
	}, std::placeholders::_1));

	// Initially, populate the widgets using an empty search
	{
		artistSearch->search(SearchFilter(), SEARCH_NB_ITEMS);
		releaseSearch->search(SearchFilter(), SEARCH_NB_ITEMS);
		trackSearch->search(SearchFilter(), SEARCH_NB_ITEMS);
	}

}

} // namespace Mobile
} // namespace UserInterface

