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
#include "LmsApplication.hpp"

#include "TrackSearch.hpp"

namespace UserInterface {
namespace Mobile {

TrackSearch::TrackSearch(Wt::WContainerWidget *parent)
: Wt::WContainerWidget(parent),
_resCount(0)
{
	Wt::WTemplate* title = new Wt::WTemplate(this);
	title->setTemplateText(Wt::WString::tr("mobile-search-title"));

	title->bindWidget("text", new Wt::WText("Tracks", Wt::PlainText));

}

void
TrackSearch::clear()
{
	while (count() > 1)
		removeWidget(this->widget(1));

	_resCount = 0;
}

void
TrackSearch::search(Database::SearchFilter filter, size_t max)
{
	clear();
	addResults(filter, max);
}

void
TrackSearch::addResults(Database::SearchFilter filter, size_t nb)
{
	Wt::Dbo::Transaction transaction(DboSession());

	std::vector< Database::Track::pointer > tracks = Database::Track::getTracks(DboSession(), filter, _resCount, nb + 1);

	bool expectMoreResults;
	if (tracks.size() == nb + 1)
	{
		expectMoreResults = true;
		tracks.pop_back();
	}
	else
		expectMoreResults = false;

	BOOST_FOREACH(Database::Track::pointer track, tracks)
	{
		Wt::WTemplate* trackWidget = new Wt::WTemplate(this);
		trackWidget->setTemplateText(Wt::WString::tr("mobile-track-res"));

		Wt::WImage *cover = new Wt::WImage();
		cover->setStyleClass("center-block");
		cover->setImageLink( Wt::WLink (LmsApplication::instance()->getCoverResource()->getTrackUrl(track.id(), 56)) );
		trackWidget->bindWidget("cover", cover);

		// Track Name (bold)
		// Artist - Album (italic)
		Wt::WContainerWidget *container = new Wt::WContainerWidget();
		Wt::WText *title = new Wt::WText(Wt::WString::fromUTF8(track->getName()), Wt::PlainText);
		title->setStyleClass("mobile-track");
		container->addWidget(title);

		if (!track->getArtistName().empty()
			|| !track->getReleaseName().empty())
		{
			title->setInline(false);
			Wt::WText *artistRelease = new  Wt::WText(Wt::WString::fromUTF8(track->getArtistName() + " - " + track->getReleaseName()), Wt::PlainText);
			artistRelease->setStyleClass("mobile-artist");
			container->addWidget(artistRelease);
		}
		trackWidget->bindWidget("name", container);

		Wt::WPushButton *playBtn = new Wt::WPushButton("Play");
		playBtn->setStyleClass("btn-primary center-block");
		playBtn->clicked().connect(std::bind([=] {
			_sigTrackPlay.emit(track.id());
		}));

		trackWidget->bindWidget("btn", playBtn);

	}

	_resCount += tracks.size();

	if (expectMoreResults)
	{
		Wt::WTemplate* moreRes = new Wt::WTemplate(this);
		moreRes->setTemplateText(Wt::WString::tr("mobile-search-more"));

		moreRes->bindWidget("text", new Wt::WText("Tap to show more results..."));

		moreRes->clicked().connect(std::bind([=] {
			_sigMoreTracksSelected();
			removeWidget(moreRes);

			addResults(filter, 20);
		}));
	}
}

} // namespace Mobile
} // namespace UserInterface
