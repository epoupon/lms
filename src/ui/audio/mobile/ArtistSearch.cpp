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
#include <Wt/WTemplate>
#include <Wt/WPushButton>
#include <Wt/WImage>

#include "LmsApplication.hpp"

#include "ArtistSearch.hpp"

namespace UserInterface {
namespace Mobile {

using namespace Database;

ArtistSearch::ArtistSearch(Wt::WContainerWidget *parent)
: Wt::WContainerWidget(parent),
_resCount(0)
{
	Wt::WTemplate *title = new Wt::WTemplate(this);
	title->setTemplateText(Wt::WString::tr("mobile-search-title"));

	title->bindWidget("text", new Wt::WText("Artists", Wt::PlainText));

	Wt::WTemplate* artistWrapper = new Wt::WTemplate(this);
	artistWrapper->setTemplateText(Wt::WString::tr("wa-artist-wrapper"));
	_contents = new Wt::WContainerWidget();
	artistWrapper->bindWidget("contents", _contents );
}

void
ArtistSearch::clear()
{
	while (count() > 1)
		removeWidget(this->widget(1));

	_resCount = 0;
}

void
ArtistSearch::search(Database::SearchFilter filter, size_t nb)
{
	clear();
	addResults(filter, nb);
}

void
ArtistSearch::addResults(Database::SearchFilter filter, std::size_t nb)
{
	Wt::Dbo::Transaction transaction(DboSession());

	std::vector<Artist::pointer> artists = Artist::getByFilter(DboSession(), filter, _resCount, nb + 1);

	bool expectMoreResults;
	if (artists.size() == nb + 1)
	{
		expectMoreResults = true;
		artists.pop_back();
	}
	else
		expectMoreResults = false;

	for (Artist::pointer artist : artists)
	{
		Wt::WTemplate* res = new Wt::WTemplate(this);
		res->setTemplateText(Wt::WString::tr("wa-artist-res"));

		Wt::WImage *artistImg = new Wt::WImage();
		artistImg->setStyleClass("center-block"); // TODO move in css?
		artistImg->setStyleClass("release_res_shadow release_img-responsive"); // TODO move in css?

		res->bindWidget("gif", artistImg);

		Wt::WText *text = new Wt::WText(Wt::WString::fromUTF8(artist->getName()), Wt::PlainText);
		res->bindWidget("name", text);

		res->clicked().connect(std::bind([=] {
			_sigArtistSelected(artist.id());
		}));
	}

	_resCount += artists.size();;

	if (expectMoreResults)
	{
		Wt::WTemplate* moreRes = new Wt::WTemplate(this);
		moreRes->setTemplateText(Wt::WString::tr("mobile-search-more"));

		moreRes->bindWidget("text", new Wt::WText("Tap to show more results..."));

		moreRes->clicked().connect(std::bind([=] {
			_sigMoreArtistsSelected();
			removeWidget(moreRes);

			addResults(filter, 20);
		}));
	}

}

} // namespace Mobile
} // namespace UserInterface
