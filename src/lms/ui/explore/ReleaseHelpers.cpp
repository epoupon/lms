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

#include "ReleaseHelpers.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WImage.h>
#include <Wt/WText.h>

#include "services/database/Artist.hpp"
#include "services/database/Release.hpp"

#include "Utils.hpp"

using namespace Database;

namespace UserInterface::ReleaseListHelpers
{
	static
	std::unique_ptr<Wt::WTemplate>
	createEntryInternal(const Release::pointer& release, const std::string& templateKey, const Artist::pointer& artist, const bool showYear)
	{
		auto entry {std::make_unique<Wt::WTemplate>(Wt::WString::tr(templateKey))};

		entry->bindWidget("release-name", Utils::createReleaseAnchor(release));
		entry->addFunction("tr", &Wt::WTemplate::Functions::tr);

		{
			Wt::WAnchor* anchor {entry->bindWidget("cover", Utils::createReleaseAnchor(release, false))};
			auto cover {Utils::createCover(release->getId(), CoverResource::Size::Large)};
			cover->addStyleClass("Lms-cover-release Lms-cover-anchor");
			anchor->setImage(std::move(cover));
		}

		auto artists {release->getReleaseArtists()};
		if (artists.empty())
			artists = release->getArtists();

		const bool isSameArtist {(std::find(std::cbegin(artists), std::cend(artists), artist) != artists.end())};

		if (artists.size() > 1)
		{
			entry->setCondition("if-has-various-artists", true);
		}
		else if (artists.size() == 1 && !isSameArtist)
		{
			entry->setCondition("if-has-artist", true);
			entry->bindWidget("artist-name", Utils::createArtistAnchor(artists.front()));
		}

		if (showYear)
		{
			Wt::WString year {ReleaseHelpers::buildReleaseYearString(release->getReleaseYear(), release->getReleaseYear(true))};
			if (!year.empty())
			{
				entry->setCondition("if-has-year", true);
				entry->bindString("year", year, Wt::TextFormat::Plain);
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

namespace UserInterface::ReleaseHelpers
{
	Wt::WString
	buildReleaseTypeString(ReleaseTypePrimary primaryType, EnumSet<ReleaseTypeSecondary> secondaryTypes)
	{
		Wt::WString res;

		switch (primaryType)
		{
			case ReleaseTypePrimary::Album:			res = Wt::WString::tr("Lms.Explore.Release.type-primary-album"); break;
			case ReleaseTypePrimary::Broadcast:		res = Wt::WString::tr("Lms.Explore.Release.type-primary-broadcast"); break;
			case ReleaseTypePrimary::EP:			res = Wt::WString::tr("Lms.Explore.Release.type-primary-ep"); break;
			case ReleaseTypePrimary::Single:		res = Wt::WString::tr("Lms.Explore.Release.type-primary-single"); break;
			case ReleaseTypePrimary::Other:			res = Wt::WString::tr("Lms.Explore.Release.type-primary-other"); break;
		}

		for (ReleaseTypeSecondary secondaryType : secondaryTypes)
		{
			res += Wt::WString {" · "};

			switch (secondaryType)
			{
				case ReleaseTypeSecondary::Compilation:		res += Wt::WString::tr("Lms.Explore.Release.type-secondary-compilation"); break;
				case ReleaseTypeSecondary::Spokenword:		res += Wt::WString::tr("Lms.Explore.Release.type-secondary-spokenword"); break;
				case ReleaseTypeSecondary::Soundtrack:		res += Wt::WString::tr("Lms.Explore.Release.type-secondary-soundtrack"); break;
				case ReleaseTypeSecondary::Interview: 		res += Wt::WString::tr("Lms.Explore.Release.type-secondary-interview"); break;
				case ReleaseTypeSecondary::Audiobook: 		res += Wt::WString::tr("Lms.Explore.Release.type-secondary-audiobook"); break;
				case ReleaseTypeSecondary::AudioDrama: 		res += Wt::WString::tr("Lms.Explore.Release.type-secondary-audiodrama"); break;
				case ReleaseTypeSecondary::Live: 			res += Wt::WString::tr("Lms.Explore.Release.type-secondary-live"); break;
				case ReleaseTypeSecondary::Remix: 			res += Wt::WString::tr("Lms.Explore.Release.type-secondary-remix"); break;
				case ReleaseTypeSecondary::DJMix: 			res += Wt::WString::tr("Lms.Explore.Release.type-secondary-djmix"); break;
				case ReleaseTypeSecondary::Mixtape_Street: 	res += Wt::WString::tr("Lms.Explore.Release.type-secondary-mixtape-street"); break;
				case ReleaseTypeSecondary::Demo: 			res += Wt::WString::tr("Lms.Explore.Release.type-secondary-demo"); break;
			}
		}

		return res;
	}

	Wt::WString buildReleaseYearString(std::optional<int> year, std::optional<int> originalYear)
	{
		Wt::WString res;

		// Year can be here, but originalYear can't be here without year (enforced by scanner)
		if (!year)
			return res;

		if (originalYear && *originalYear != *year)
			res = std::to_string(*originalYear) + " (" + std::to_string(*year) + ")";
		else
			res = std::to_string(*year);

		return res;
	}
} // namespace UserInterface::ReleaseHelpers}
