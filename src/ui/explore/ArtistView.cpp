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

#include <Wt/WAnchor.h>
#include <Wt/WImage.h>
#include <Wt/WTemplate.h>
#include <Wt/WText.h>

#include "database/Types.hpp"

#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

#include "resource/ImageResource.hpp"

#include "LmsApplication.hpp"
#include "Filters.hpp"
#include "ArtistView.hpp"

namespace UserInterface {

Artist::Artist(Filters* filters)
: _filters(filters)
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

	auto artistId = readAs<Database::Artist::id_type>(wApp->internalPathNextPart("/artist/"));
	if (!artistId)
		return;

        Wt::Dbo::Transaction transaction(LmsApp->getDboSession());
	auto artist = Database::Artist::getById(LmsApp->getDboSession(), *artistId);
	if (!artist)
	{
		LmsApp->goHome();
		return;
	}

	Wt::WTemplate* t = addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Artist.template"));
	t->addFunction("tr", &Wt::WTemplate::Functions::tr);

	Wt::WContainerWidget* clusterContainers = t->bindNew<Wt::WContainerWidget>("clusters");

	{
		auto clusters = artist->getClusters(3);

		for (auto cluster : clusters)
		{
			Wt::WTemplate* entry = clusterContainers->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Artist.template.cluster-entry"));
			entry->bindString("name", Wt::WString::fromUTF8(cluster->getName()), Wt::TextFormat::Plain);
		}
	}

	t->bindString("name", Wt::WString::fromUTF8(artist->getName()), Wt::TextFormat::Plain);
	{
		Wt::WText* playBtn = t->bindNew<Wt::WText>("play-btn", Wt::WString::tr("Lms.Explore.Artist.play"), Wt::TextFormat::XHTML);

		playBtn->clicked().connect(std::bind([=]
		{
			artistPlay.emit(*artistId);
		}));
	}

	{
		Wt::WText* addBtn = t->bindNew<Wt::WText>("add-btn", Wt::WString::tr("Lms.Explore.Artist.add"), Wt::TextFormat::XHTML);

		addBtn->clicked().connect(std::bind([=]
		{
			artistAdd.emit(*artistId);
		}));
	}

	Wt::WContainerWidget* releasesContainer = t->bindNew<Wt::WContainerWidget>("releases");

	auto releases = artist->getReleases(_filters->getClusterIds());
	for (auto release : releases)
	{
		auto releaseId = release.id();

		Wt::WTemplate* entry = releasesContainer->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Artist.template.entry"));
		entry->addFunction("tr", Wt::WTemplate::Functions::tr);

		{
			Wt::WAnchor* anchor = entry->bindWidget("cover", LmsApplication::createReleaseAnchor(release, false));

			auto cover = std::make_unique<Wt::WImage>();
			cover->setImageLink(LmsApp->getImageResource()->getReleaseUrl(release.id(), 128));
			// Some images may not be square
			cover->setWidth(128);
			anchor->setImage(std::move(cover));
		}

		entry->bindWidget("name", LmsApplication::createReleaseAnchor(release));

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

		Wt::WText* playBtn = entry->bindNew<Wt::WText>("play-btn", Wt::WString::tr("Lms.Explore.Artist.play"), Wt::TextFormat::XHTML);
		playBtn->clicked().connect(std::bind([=]
		{
			releasePlay.emit(releaseId);
		}));

		Wt::WText* addBtn = entry->bindNew<Wt::WText>("add-btn", Wt::WString::tr("Lms.Explore.Artist.add"), Wt::TextFormat::XHTML);
		addBtn->clicked().connect(std::bind([=]
		{
			releaseAdd.emit(releaseId);
		}));
	}
}

} // namespace UserInterface

