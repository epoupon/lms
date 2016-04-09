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

TrackSearch::TrackSearch(PlayQueueEvents& events, Wt::WContainerWidget *parent)
: Wt::WContainerWidget(parent),
 _events(events)
{
	Wt::WTemplate* t = new Wt::WTemplate(this);
	t->setTemplateText(Wt::WString::tr("wa-track-search"));

	Wt::WTemplate* title = new Wt::WTemplate(this);
	title->setTemplateText(Wt::WString::tr("mobile-search-title"));
	title->bindString("text", "Tracks", Wt::PlainText);
	t->bindWidget("title", title);

	_contents = new Wt::WContainerWidget();
	t->bindWidget("contents", _contents);

	_showMore = new Wt::WTemplate();
	_showMore->setTemplateText(Wt::WString::tr("mobile-search-more"));
	_showMore->bindString("text", "Tap to show more results...");
	_showMore->hide();
	_showMore->clicked().connect(std::bind([=] {
		_sigShowMore.emit();
		addResults(20);
	}));
	t->bindWidget("show-more", _showMore);
}

void
TrackSearch::clear()
{
	_contents->clear();
	_showMore->hide();
}

void
TrackSearch::search(SearchFilter filter, size_t nb)
{
	_filter = filter;

	clear();
	addResults(nb);
}

void
TrackSearch::addResults(size_t nb)
{
	Wt::Dbo::Transaction transaction(DboSession());

	bool moreResults;
	std::vector<Track::pointer > tracks = Track::getByFilter(DboSession(), _filter, _contents->count(), nb, moreResults);

	for (Track::pointer track : tracks)
	{
		Wt::WTemplate* res = new Wt::WTemplate(_contents);
		res->setTemplateText(Wt::WString::tr("wa-track-search-res"));

		Wt::WImage *cover = new Wt::WImage();
		res->bindWidget("cover", cover);
		cover->setStyleClass ("center-block img-responsive");
		cover->setImageLink(SessionImageResource()->getTrackUrl(track.id(), 64));
 		res->bindString("track-name", Wt::WString::fromUTF8(track->getName()), Wt::PlainText);
 		res->bindString("artist-name", Wt::WString::fromUTF8(track->getArtist()->getName()), Wt::PlainText);

		Wt::WText *playBtn = new Wt::WText("<i class=\"fa fa-play fa-lg\"></i>", Wt::XHTMLText);
		res->bindWidget("play-btn", playBtn);
		playBtn->setStyleClass("mobile-btn");
		playBtn->clicked().connect(std::bind([=] {
			_events.trackPlay.emit(track.id());
		}));

		Wt::WText *addBtn = new Wt::WText("<i class=\"fa fa-plus fa-lg\"></i>", Wt::XHTMLText);
		res->bindWidget("add-btn", addBtn);
		addBtn->setStyleClass("mobile-btn");
		addBtn->clicked().connect(std::bind([=] {
			_events.trackAdd.emit(track.id());
		}));


	}

	if (moreResults)
		_showMore->show();
	else
		_showMore->hide();
}

} // namespace Mobile
} // namespace UserInterface
