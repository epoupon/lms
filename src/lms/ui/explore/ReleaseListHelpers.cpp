/*
 * Copyright (C) 2019 Emeric Poupon
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

#include "ReleaseListHelpers.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WImage.h>
#include <Wt/WText.h>

#include "services/database/Artist.hpp"
#include "services/database/Release.hpp"
#include "resource/CoverResource.hpp"

#include "LmsApplication.hpp"

using namespace Database;

namespace UserInterface::ReleaseListHelpers
{
	static
	std::unique_ptr<Wt::WTemplate>
	createEntryInternal(const Release::pointer& release, const std::string& templateKey, const Artist::pointer& artist, const bool showYear)
	{
		auto entry {std::make_unique<Wt::WTemplate>(Wt::WString::tr(templateKey))};

		entry->bindWidget("release-name", LmsApplication::createReleaseAnchor(release));
		entry->addFunction("tr", &Wt::WTemplate::Functions::tr);

		Wt::WAnchor* anchor = entry->bindWidget("cover", LmsApplication::createReleaseAnchor(release, false));
		auto cover = std::make_unique<Wt::WImage>();
		cover->setImageLink(LmsApp->getCoverResource()->getReleaseUrl(release->getId(), CoverResource::Size::Large));
		cover->setStyleClass("Lms-cover");
		cover->setAttributeValue("onload", LmsApp->javaScriptClass() + ".onLoadCover(this)");
		anchor->setImage(std::move(cover));

		auto artists = release->getReleaseArtists();
		if (artists.empty())
			artists = release->getArtists();

		bool isSameArtist {(std::find(std::cbegin(artists), std::cend(artists), artist) != artists.end())};

		if (artists.size() > 1)
		{
			entry->setCondition("if-has-various-artists", true);
		}
		else if (artists.size() == 1 && !isSameArtist)
		{
			entry->setCondition("if-has-artist", true);
			entry->bindWidget("artist-name", LmsApplication::createArtistAnchor(artists.front()));
		}

		if (showYear)
		{
			if (std::optional<int> year {release->getReleaseYear()})
			{
				entry->setCondition("if-has-year", true);

				std::string strYear {std::to_string(*year)};

				std::optional<int> originalYear {release->getReleaseYear(true)};
				if (originalYear && *originalYear != *year)
				{
					strYear += " (" + std::to_string(*originalYear) + ")";
				}

				entry->bindString("year", strYear);
			}
		}

		return entry;
	}

	std::unique_ptr<Wt::WTemplate>
	createEntry(const Release::pointer& release, const Artist::pointer& artist, bool showYear)
	{
		return createEntryInternal(release, "Lms.Explore.Releases.template.entry-grid", artist, showYear);
	}

	std::unique_ptr<Wt::WTemplate>
	createEntry(const Release::pointer& release)
	{
		return createEntry(release, Artist::pointer {}, false /*year*/);
	}

	std::unique_ptr<Wt::WTemplate>
	createEntryForArtist(const Database::Release::pointer& release, const Database::Artist::pointer& artist)
	{
		return createEntry(release, artist, true);
	}
} // namespace UserInterface

