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

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>

#include <Wt/WContainerWidget>
#include <Wt/WTemplate>
#include <Wt/WLineEdit>
#include <Wt/WText>
#include <Wt/WTable>

#include "MobileAudio.hpp"

#include "ArtistSearch.hpp"
#include "ReleaseSearch.hpp"
#include "TrackSearch.hpp"
#include "MobileAudioMediaPlayer.hpp"

#include "logger/Logger.hpp"

namespace UserInterface {
namespace Mobile {

Audio::Audio(Database::Handler& db, Wt::WContainerWidget *parent)
: UserInterface::Audio(parent),
_db(db)
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

	ArtistSearch* artistSearch = new ArtistSearch(_db, this);
	ReleaseSearch* releaseSearch = new ReleaseSearch(_db, this);
	TrackSearch* trackSearch = new TrackSearch(_db, this);

	Wt::WTemplate* footer = new Wt::WTemplate(this);
	footer->setTemplateText(Wt::WString::tr("mobile-audio-footer"));

	Wt::Dbo::Transaction transaction(_db.getSession());
	Database::User::pointer user = _db.getCurrentUser();

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

	AudioMediaPlayer* mediaPlayer = new AudioMediaPlayer(db, encoding);
	footer->bindWidget("player", mediaPlayer);

	edit->changed().connect(std::bind([=] () {
		std::string text = edit->text().toUTF8();

		// When a new search is done, output some results from:
		// Artist
		// Release
		// Song
		std::vector<std::string> keywords;
		boost::algorithm::split(keywords, text, boost::is_any_of(" "), boost::token_compress_on);

		{
			Database::SearchFilter filter;
			BOOST_FOREACH(std::string keyword, keywords)
			{
				Database::SearchFilter::FieldValues likeMatch;

				likeMatch[Database::SearchFilter::Field::Artist].push_back(keyword);
				likeMatch[Database::SearchFilter::Field::Release].push_back(keyword);
				filter.likeMatches.push_back(likeMatch);
			}

			releaseSearch->search(filter, 3);
		}

		{
			Database::SearchFilter filter;
			BOOST_FOREACH(std::string keyword, keywords)
			{
				Database::SearchFilter::FieldValues likeMatch;

				likeMatch[Database::SearchFilter::Field::Artist].push_back(keyword);

				filter.likeMatches.push_back(likeMatch);
			}

			artistSearch->search(filter, 3);
		}

		{
			Database::SearchFilter filter;
			BOOST_FOREACH(std::string keyword, keywords)
			{
				Database::SearchFilter::FieldValues likeMatch;

				likeMatch[Database::SearchFilter::Field::Track].push_back(keyword);

				filter.likeMatches.push_back(likeMatch);
			}

			trackSearch->search(filter, 3);
		}

		artistSearch->show();
		releaseSearch->show();
		trackSearch->show();

	}));

	artistSearch->moreArtistsSelected().connect(std::bind([=] {
		releaseSearch->hide();
		trackSearch->hide();
		artistSearch->show();
	}));

	artistSearch->artistSelected().connect(std::bind([=] (std::string artist) {
		artistSearch->hide();
		trackSearch->hide();
		releaseSearch->show();

		Database::SearchFilter filter;
		filter.exactMatch[Database::SearchFilter::Field::Artist].push_back(artist);

		releaseSearch->search(filter, 20);
	}, std::placeholders::_1));

	releaseSearch->moreReleasesSelected().connect(std::bind([=] {
		artistSearch->hide();
		trackSearch->hide();
		releaseSearch->show();
	}));

	releaseSearch->releaseSelected().connect(std::bind([=] (std::string release)
	{
		artistSearch->hide();
		releaseSearch->hide();
		trackSearch->show();

		// TODO load track search with selected release
		Database::SearchFilter filter;
		filter.exactMatch[Database::SearchFilter::Field::Release].push_back(release);

		trackSearch->search(filter, 20);
	}, std::placeholders::_1));

	trackSearch->moreTracksSelected().connect(std::bind([=]
	{
		artistSearch->hide();
		releaseSearch->hide();
	}));

	trackSearch->trackPlay().connect(std::bind([=] (Database::Track::id_type id)
	{
		LMS_LOG(MOD_UI, SEV_DEBUG) << "Playing track id " << id;
		Wt::Dbo::Transaction transaction(_db.getSession());

		Database::Track::pointer track = Database::Track::getById(_db.getSession(), id);

		if (track)
		{
			// Determine the output format using the encoding of the player
			Transcode::Format::Encoding encoding;
			switch(mediaPlayer->getEncoding())
			{
				case Wt::WMediaPlayer::MP3: encoding = Transcode::Format::MP3; break;
				case Wt::WMediaPlayer::FLA: encoding = Transcode::Format::FLA; break;
				case Wt::WMediaPlayer::OGA: encoding = Transcode::Format::OGA; break;
				case Wt::WMediaPlayer::WEBMA: encoding = Transcode::Format::WEBMA; break;
				default:
					encoding = Transcode::Format::MP3;
			}
			// TODO compute parameters using user s profile
			Transcode::InputMediaFile inputFile(track->getPath());
			Transcode::Parameters parameters(inputFile, Transcode::Format::get(encoding));
			parameters.setBitrate(Transcode::Stream::Audio, 96000);

			mediaPlayer->play(track.id(), parameters);
		}
	} , std::placeholders::_1));

	// Initially, populate the widgets using an empty search
	{
		Database::SearchFilter filter; // empty filter
		artistSearch->search(filter, 3);
		releaseSearch->search(filter, 3);
		trackSearch->search(filter, 3);
	}

}

} // namespace Mobile
} // namespace UserInterface

