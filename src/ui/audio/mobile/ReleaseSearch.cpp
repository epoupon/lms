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

namespace UserInterface {
namespace Mobile {

using namespace Database;

ReleaseSearch::ReleaseSearch(Wt::WContainerWidget *parent)
: Wt::WContainerWidget(parent),
_resCount(0)
{
	Wt::WTemplate* title = new Wt::WTemplate(this);
	title->setTemplateText(Wt::WString::tr("mobile-search-title"));

	title->bindWidget("text", new Wt::WText("Releases", Wt::PlainText));

	Wt::WTemplate* releaseWrapper = new Wt::WTemplate(this);
	releaseWrapper->setTemplateText(Wt::WString::tr("wa-release-wrapper"));
	_contents = new Wt::WContainerWidget();
	releaseWrapper->bindWidget("contents", _contents );
}

void
ReleaseSearch::clear()
{
	while (count() > 1)
		removeWidget(this->widget(1));

	_resCount = 0;
}

void
ReleaseSearch::search(Database::SearchFilter filter, size_t max)
{
	clear();
	addResults(filter, max);
}

static  Wt::WString
getArtistFromRelease(Release::pointer release)
{
	auto artists = Artist::getByFilter(DboSession(),
			SearchFilter::ById(SearchFilter::Field::Release, release.id()), -1, 2);

	if (artists.size() > 1)
		return Wt::WString::fromUTF8("Various artists", Wt::PlainText);
	else
		return Wt::WString::fromUTF8(artists.front()->getName(), Wt::PlainText);
}

void
ReleaseSearch::addResults(Database::SearchFilter filter, size_t nb)
{
	using namespace Database;

	Wt::Dbo::Transaction transaction(DboSession());

	std::vector<Release::pointer> releases = Release::getByFilter(DboSession(), filter, _resCount, nb + 1);

	bool expectMoreResults;
	if (releases.size() == nb + 1)
	{
		expectMoreResults = true;
		releases.pop_back();
	}
	else
		expectMoreResults = false;

	for (Release::pointer release : releases)
	{
		Wt::WTemplate* releaseWidget = new Wt::WTemplate(this);
		releaseWidget->setTemplateText(Wt::WString::tr("wa-release-res"));

		Wt::WAnchor *coverAnchor = new Wt::WAnchor(Wt::WLink(Wt:: WLink::InternalPath, "/audio/release/" + std::to_string(release.id())));
		Wt::WImage *cover = new Wt::WImage(coverAnchor);
		cover->setStyleClass("center-block");
		cover->setImageLink( Wt::WLink( LmsApplication::instance()->getCoverResource()->getReleaseUrl(release.id(), 512)));
		cover->setStyleClass("release_res_shadow release_img-responsive"); // TODO move?

		releaseWidget->bindWidget("cover", coverAnchor);
		releaseWidget->bindWidget("name", new Wt::WText(Wt::WString::fromUTF8(release->getName()), Wt::PlainText));
		releaseWidget->bindString("release_name", Wt::WString::fromUTF8(release->getName()), Wt::PlainText);


		auto artists = Artist::getByFilter(DboSession(),
			SearchFilter::ById(SearchFilter::Field::Release, release.id()), -1, 2);
		if (artists.size() == 1)
		{
			Wt::WAnchor *artistAnchor = new Wt::WAnchor(Wt::WLink(Wt:: WLink::InternalPath, "/audio/artist/" + std::to_string(artists.front().id())));
			Wt::WText *artist = new Wt::WText(artistAnchor);
			artist->setText(Wt::WString::fromUTF8(artists.front()->getName(), Wt::PlainText));
			releaseWidget->bindWidget("artist", artistAnchor);
		}
		else
			releaseWidget->bindWidget("artist", new Wt::WText(Wt::WString::fromUTF8("Various Artists", Wt::PlainText)));

	}

	_resCount += releases.size();;

	if (expectMoreResults)
	{
		Wt::WTemplate* moreRes = new Wt::WTemplate(this);
		moreRes->setTemplateText(Wt::WString::tr("mobile-search-more"));

		moreRes->bindWidget("text", new Wt::WText("Tap to show more results..."));

		moreRes->clicked().connect(std::bind([=] {
			_sigMoreReleasesSelected();
			removeWidget(moreRes);

			addResults(filter, 20);
		}));
	}
}

} // namespace Mobile
} // namespace UserInterface
