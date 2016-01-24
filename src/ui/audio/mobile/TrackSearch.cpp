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

#include <Wt/WText>
#include <Wt/WImage>
#include <Wt/WPushButton>
#include <Wt/WTemplate>

#include "logger/Logger.hpp"
#include "utils/Utils.hpp"
#include "LmsApplication.hpp"

#include "TrackSearch.hpp"

namespace UserInterface {
namespace Mobile {

using namespace Database;

TrackSearch::TrackSearch(Wt::WContainerWidget *parent)
: Wt::WContainerWidget(parent)
{
	Wt::WTemplate* wrapper = new Wt::WTemplate(this);
	wrapper->setTemplateText(Wt::WString::tr("wa-track-wrapper"));

	Wt::WTemplate* title = new Wt::WTemplate(this);
	wrapper->bindWidget("title", title);

	title->setTemplateText(Wt::WString::tr("mobile-search-title"));
	title->bindString("text", "Tracks", Wt::PlainText);

	_container = new Wt::WContainerWidget();
	wrapper->bindWidget("track-container", _container);

	_showMore = new Wt::WTemplate();
	wrapper->bindWidget("show-more", _showMore);

	_showMore->setTemplateText(Wt::WString::tr("mobile-search-more"));
	_showMore->bindString("text", "Tap to show more results...");
	_showMore->hide();
	_showMore->clicked().connect(std::bind([=] {
		_sigMoreSelected();
		addResults(20);
	}));
}

void
TrackSearch::clear()
{
	// Flush the release container
	_container->clear();

	_nbTracks = 0;
	_showMore->hide();
}

void
TrackSearch::search(SearchFilter filter, size_t nb)
{
	_filter = filter;

	clear();
	addResults(nb);
}

static
std::vector<Track::pointer >
getTracks(SearchFilter filter, size_t offset, size_t nb, bool &moreResults)
{
	std::vector<Track::pointer > tracks = Track::getByFilter(DboSession(), filter, offset, nb + 1);

	if (tracks.size() == nb + 1)
	{
		moreResults = true;
		tracks.pop_back();
	}
	else
		moreResults = false;

	return tracks;
}

void
TrackSearch::addResults(size_t nb)
{
	Wt::Dbo::Transaction transaction(DboSession());

	bool moreResults;
	std::vector<Track::pointer > tracks = getTracks(_filter, _nbTracks, nb, moreResults);

	for (Track::pointer track : tracks)
	{
		Wt::WTemplate* trackRes = new Wt::WTemplate(_container);
		trackRes->setTemplateText(Wt::WString::tr("wa-track"));

		Wt::WImage *cover = new Wt::WImage();
		cover->setStyleClass ("center-block img-responsive");
		cover->setImageLink(Wt::WLink(LmsApplication::instance()->getCoverResource()->getTrackUrl(track.id(), 128)));
		trackRes->bindWidget("cover", cover);

 		trackRes->bindString("track-name", Wt::WString::fromUTF8(track->getName()), Wt::PlainText);
 		trackRes->bindString("artist-name", Wt::WString::fromUTF8(track->getArtist()->getName()), Wt::PlainText);

		Wt::WText *playBtn = new Wt::WText("Play", Wt::PlainText);
		playBtn->setStyleClass("center-block"); // TODO move to CSS?
		playBtn->clicked().connect(std::bind([=] {
			_sigTrackPlay.emit(track.id());
		}));

		trackRes->bindWidget("btn", playBtn);
		_nbTracks++;
	}

	if (moreResults)
		_showMore->show();
	else
		_showMore->hide();
}

} // namespace Mobile
} // namespace UserInterface
