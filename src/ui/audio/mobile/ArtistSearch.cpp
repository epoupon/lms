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

#include "ArtistSearch.hpp"

namespace UserInterface {
namespace Mobile {

ArtistSearch::ArtistSearch(Database::Handler& db, Wt::WContainerWidget *parent)
: Wt::WContainerWidget(parent),
_db(db),
_resCount(0)
{
	Wt::WTemplate *title = new Wt::WTemplate(this);
	title->setTemplateText(Wt::WString::tr("mobile-search-title"));

	title->bindWidget("text", new Wt::WText("Artists", Wt::PlainText));
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

	std::vector<std::string> artists;

	{
		Wt::Dbo::Transaction transaction(_db.getSession());

		// Request one more to see if more results are to be expected
		artists = Database::Track::getArtists(_db.getSession(), filter, _resCount, nb + 1);
	}

	bool expectMoreResults;
	if (artists.size() == nb + 1)
	{
		expectMoreResults = true;
		artists.pop_back();
	}
	else
		expectMoreResults = false;

	BOOST_FOREACH(std::string artist, artists)
	{
		Wt::WTemplate* res = new Wt::WTemplate(this);
		res->setTemplateText(Wt::WString::tr("mobile-artist-res"));

		Wt::WText *text = new Wt::WText(Wt::WString::fromUTF8(artist), Wt::PlainText);
		res->bindWidget("name", text);

		res->clicked().connect(std::bind([=] {
			_sigArtistSelected(artist);
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
