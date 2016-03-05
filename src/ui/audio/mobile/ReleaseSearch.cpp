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

#include "ReleaseSearch.hpp"
#include "ReleaseView.hpp"
#include "ArtistView.hpp"

namespace UserInterface {
namespace Mobile {

using namespace Database;

ReleaseSearch::ReleaseSearch(Wt::WContainerWidget *parent)
: Wt::WContainerWidget(parent)
{
	Wt::WTemplate* t = new Wt::WTemplate(this);
	t->setTemplateText(Wt::WString::tr("wa-release-search"));

	Wt::WTemplate *titleTemplate = new Wt::WTemplate(this);
	titleTemplate->setTemplateText(Wt::WString::tr("mobile-search-title"));

	_title = new Wt::WText();
	titleTemplate->bindWidget("text", _title);

	t->bindWidget("title", titleTemplate);

	_contents = new Wt::WContainerWidget();
	t->bindWidget("contents", _contents );

	_showMore = new Wt::WTemplate();
	_showMore->setTemplateText(Wt::WString::tr("mobile-search-more"));
	_showMore->bindString("text", "Tap to show more results...");
	_showMore->hide();
	_showMore->clicked().connect(std::bind([=] {
		_sigShowMore.emit();
	}));

	t->bindWidget("show-more", _showMore);
}

void
ReleaseSearch::clear()
{
	_contents->clear();
	_showMore->hide();
}

void
ReleaseSearch::search(Database::SearchFilter filter, size_t max, Wt::WString title)
{
	_filter = filter;
	_title->setText(title);

	clear();
	addResults(max);
}

void
ReleaseSearch::addResults(size_t nb)
{
	using namespace Database;

	Wt::Dbo::Transaction transaction(DboSession());

	bool moreResults;
	std::vector<Release::pointer> releases = Release::getByFilter(DboSession(), _filter, _contents->count(), nb, moreResults);

	for (Release::pointer release : releases)
	{
		Wt::WTemplate* res = new Wt::WTemplate(_contents);
		res->setTemplateText(Wt::WString::tr("wa-release-search-res"));

		Wt::WAnchor *coverAnchor = new Wt::WAnchor(ReleaseView::getLink(release.id()));
		Wt::WImage *cover = new Wt::WImage(coverAnchor);
		cover->setStyleClass("center-block");
		cover->setImageLink( SessionImageResource()->getReleaseUrl(release.id(), 512));
		cover->setStyleClass("release_res_shadow release_img-responsive"); // TODO move?

		res->bindWidget("cover", coverAnchor);
		res->bindWidget("name", new Wt::WText(Wt::WString::fromUTF8(release->getName()), Wt::PlainText));
		res->bindString("release_name", Wt::WString::fromUTF8(release->getName()), Wt::PlainText);

		auto artists = Artist::getByFilter(DboSession(),
			SearchFilter::ById(SearchFilter::Field::Release, release.id()), -1, 2);
		if (artists.size() == 1)
		{
			Wt::WAnchor *artistAnchor = new Wt::WAnchor(ArtistView::getLink(artists.front().id()));
			Wt::WText *artist = new Wt::WText(artistAnchor);
			artist->setText(Wt::WString::fromUTF8(artists.front()->getName(), Wt::PlainText));
			res->bindWidget("artist", artistAnchor);
		}
		else
			res->bindWidget("artist", new Wt::WText(Wt::WString::fromUTF8("Various Artists", Wt::PlainText)));

	}

	if (moreResults)
		_showMore->show();
	else
		_showMore->hide();

}

} // namespace Mobile
} // namespace UserInterface
