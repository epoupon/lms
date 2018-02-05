/*
 * Copyright (C) 2018 Emeric Poupon
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

#include <Wt/WApplication>
#include <Wt/WAnchor>
#include <Wt/WImage>
#include <Wt/WTemplate>
#include <Wt/WText>

#include "database/Types.hpp"

#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

#include "LmsApplication.hpp"
#include "ArtistView.hpp"

namespace UserInterface {

Artist::Artist(Filters* filters, Wt::WContainerWidget* parent)
: Wt::WContainerWidget(parent),
  _filters(filters)
{
	wApp->internalPathChanged().connect(std::bind([=]
	{
		refresh();
	}));

	refresh();

	filters->updated().connect(std::bind([=] {
		refresh();
	}));
}

void
Artist::refresh()
{
	if (!wApp->internalPathMatches("/artist/"))
		return;

	clear();

	Database::Artist::id_type artistId;
	if (!readAs(wApp->internalPathNextPart("/artist/"), artistId))
		return;

        Wt::Dbo::Transaction transaction(DboSession());
	auto artist = Database::Artist::getById(DboSession(), artistId);

	if (!artist)
	{
		LmsApp->goHome();
		return;
	}

	auto t = new Wt::WTemplate(Wt::WString::tr("template-artist"), this);
	t->addFunction("tr", &Wt::WTemplate::Functions::tr);

	t->bindString("name", Wt::WString::fromUTF8(artist->getName()), Wt::PlainText);

	auto releasesContainer = new Wt::WContainerWidget();
	t->bindWidget("releases", releasesContainer);

	auto releases = artist->getReleases(_filters->getClusterIds());
	for (auto release : releases)
	{
		if (release->isNone())
			continue;

		Wt::WTemplate* entry = new Wt::WTemplate(Wt::WString::tr("template-artist-entry"), releasesContainer);
		entry->addFunction("tr", Wt::WTemplate::Functions::tr);

		{
			Wt::WAnchor* coverAnchor = new Wt::WAnchor(Wt::WLink(Wt::WLink::InternalPath, "/release/" + std::to_string(release.id())));
			Wt::WImage* cover = new Wt::WImage(coverAnchor);
			cover->setImageLink(SessionImageResource()->getReleaseUrl(release.id(), 128));
			// Some images may not be square
			cover->setWidth(128);
			entry->bindWidget("cover", coverAnchor);
		}

		{
			Wt::WAnchor* releaseAnchor = new Wt::WAnchor(Wt::WLink(Wt::WLink::InternalPath, "/release/" + std::to_string(release.id())));
			Wt::WText* releaseName = new Wt::WText(releaseAnchor);
			releaseName->setText(Wt::WString::fromUTF8(release->getName(), Wt::PlainText));
			entry->bindWidget("name", releaseAnchor);
		}

		if (release->hasVariousArtists())
			entry->setCondition("if-has-various-artists", true);

		boost::optional<int> year = release->getReleaseYear();
		if (year)
		{
			entry->setCondition("if-has-year", true);
			entry->bindInt("year", *year);

			boost::optional<int> originalYear = release->getReleaseYear(true);
			if (originalYear && *originalYear != *year)
			{
				entry->setCondition("if-has-orig-year", true);
				entry->bindInt("orig-year", *originalYear);
			}
		}
	}
}

} // namespace UserInterface

