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

#include "LmsApplication.hpp"

#include "ArtistSearch.hpp"
#include "ReleaseSearch.hpp"
#include "TrackSearch.hpp"
#include "MobileAudioMediaPlayer.hpp"

#include "MobileAudio.hpp"

#define SEARCH_NB_ITEMS	4

namespace UserInterface {
namespace Mobile {

using namespace Database;

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

	Wt::WTemplate* footer = new Wt::WTemplate(this);
	footer->setTemplateText(Wt::WString::tr("mobile-audio-footer"));

	// Determine the encoding to be used
	Wt::WMediaPlayer::Encoding encoding;

	{
		Wt::Dbo::Transaction transaction (DboSession());

		switch (CurrentUser()->getAudioEncoding())
		{
			case AudioEncoding::MP3: encoding = Wt::WMediaPlayer::MP3; break;
			case AudioEncoding::WEBMA: encoding = Wt::WMediaPlayer::WEBMA; break;
			case AudioEncoding::OGA: encoding = Wt::WMediaPlayer::OGA; break;
			case AudioEncoding::FLA: encoding = Wt::WMediaPlayer::FLA; break;
			case AudioEncoding::AUTO:
			default:
			   encoding = AudioMediaPlayer::getBestEncoding();
		}
	}

	AudioMediaPlayer* mediaPlayer = new AudioMediaPlayer(encoding);
	footer->bindWidget("player", mediaPlayer);

	edit->changed().connect(std::bind([=] ()
	{
		// When a new search is done, output some results from:
		// Artist
		// Release
		// Song
		auto keywords = splitString(edit->text().toUTF8(), " ");;

		releaseSearch->search(SearchFilter::ByName(SearchFilter::Field::Release, keywords), SEARCH_NB_ITEMS);
		artistSearch->search(SearchFilter::ByName(SearchFilter::Field::Artist, keywords), SEARCH_NB_ITEMS);
		trackSearch->search(SearchFilter::ByName(SearchFilter::Field::Track, keywords), SEARCH_NB_ITEMS);

		artistSearch->show();
		releaseSearch->show();
		trackSearch->show();

	}));

	artistSearch->moreArtistsSelected().connect(std::bind([=]
	{
		releaseSearch->hide();
		trackSearch->hide();
		artistSearch->show();
	}));

	artistSearch->artistSelected().connect(std::bind([=] (Artist::id_type artistId)
	{
		artistSearch->hide();
		trackSearch->hide();
		releaseSearch->show();

		releaseSearch->search(SearchFilter::ById(SearchFilter::Field::Artist, artistId), 20);
	}, std::placeholders::_1));

	releaseSearch->moreReleasesSelected().connect(std::bind([=]
	{
		artistSearch->hide();
		trackSearch->hide();
		releaseSearch->show();
	}));

	releaseSearch->releaseSelected().connect(std::bind([=] (Release::id_type releaseId)
	{
		artistSearch->hide();
		releaseSearch->hide();
		trackSearch->show();

		trackSearch->search(SearchFilter::ById(SearchFilter::Field::Release, releaseId), 20);
	}, std::placeholders::_1));

	trackSearch->moreTracksSelected().connect(std::bind([=]
	{
		artistSearch->hide();
		releaseSearch->hide();
	}));

	trackSearch->trackPlay().connect(std::bind([=] (Track::id_type id)
	{
		LMS_LOG(UI, DEBUG) << "Playing track id " << id;
		// TODO reduce transaction scope here
		Wt::Dbo::Transaction transaction(DboSession());

		Track::pointer track = Track::getById(DboSession(), id);

		if (track)
		{
			Av::TranscodeParameters parameters;

			// Determine the output format using the encoding of the player
			Av::Encoding encoding;
			switch(mediaPlayer->getEncoding())
			{
			case Wt::WMediaPlayer::MP3: encoding = Av::Encoding::MP3; break;
			case Wt::WMediaPlayer::FLA: encoding = Av::Encoding::FLA; break;
			case Wt::WMediaPlayer::OGA: encoding = Av::Encoding::OGA; break;
			case Wt::WMediaPlayer::WEBMA: encoding = Av::Encoding::WEBMA; break;
				default:
					encoding = Av::Encoding::MP3;
			}
			parameters.setEncoding(encoding);

			// TODO compute parameters using user's profile
			parameters.setBitrate(Av::Stream::Type::Audio, 96000);

			mediaPlayer->play(track.id(), parameters);
		}
	} , std::placeholders::_1));

	// Initially, populate the widgets using an empty search
	{
		artistSearch->search(SearchFilter(), SEARCH_NB_ITEMS);
		releaseSearch->search(SearchFilter(), SEARCH_NB_ITEMS);
		trackSearch->search(SearchFilter(), SEARCH_NB_ITEMS);
	}

}

} // namespace Mobile
} // namespace UserInterface

