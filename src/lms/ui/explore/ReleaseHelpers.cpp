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

#include "database/Artist.hpp"
#include "database/Release.hpp"

#include "Utils.hpp"

namespace lms::ui::releaseListHelpers
{
    using namespace db;

    namespace
    {
        std::unique_ptr<Wt::WTemplate> createEntryInternal(const Release::pointer& release, const std::string& templateKey, const Artist::pointer& artist, const bool showYear)
        {
            auto entry{ std::make_unique<Wt::WTemplate>(Wt::WString::tr(templateKey)) };

            entry->bindWidget("release-name", utils::createReleaseAnchor(release));
            entry->addFunction("tr", &Wt::WTemplate::Functions::tr);

            {
                Wt::WAnchor* anchor{ entry->bindWidget("cover", utils::createReleaseAnchor(release, false)) };
                auto image{ utils::createReleaseCover(release->getId(), ArtworkResource::Size::Large) };
                image->addStyleClass("Lms-cover-release Lms-cover-anchor"); // hack
                anchor->setImage(std::move(image));
            }

            auto artistAnchors{ utils::createArtistsAnchorsForRelease(release, artist ? artist->getId() : ArtistId{}, "link-secondary") };
            if (artistAnchors)
            {
                entry->setCondition("if-has-artist", true);
                entry->bindWidget("artist-name", std::move(artistAnchors));
            }

            if (showYear)
            {
                Wt::WString year{ releaseHelpers::buildReleaseYearString(release->getYear(), release->getOriginalYear()) };
                if (!year.empty())
                {
                    entry->setCondition("if-has-year", true);
                    entry->bindString("year", year, Wt::TextFormat::Plain);
                }
            }

            return entry;
        }
    } // namespace

    std::unique_ptr<Wt::WTemplate> createEntry(const Release::pointer& release, const Artist::pointer& artist, bool showYear)
    {
        return createEntryInternal(release, "Lms.Explore.Releases.template.entry-grid", artist, showYear);
    }

    std::unique_ptr<Wt::WTemplate> createEntry(const Release::pointer& release)
    {
        return createEntry(release, Artist::pointer{}, false /*year*/);
    }

    std::unique_ptr<Wt::WTemplate> createEntryForArtist(const db::Release::pointer& release, const db::Artist::pointer& artist)
    {
        return createEntry(release, artist, true);
    }
} // namespace lms::ui::releaseListHelpers

namespace lms::ui::releaseHelpers
{
    Wt::WString buildReleaseTypeString(const ReleaseType& releaseType)
    {
        Wt::WString res;

        if (releaseType.primaryType)
        {
            switch (*releaseType.primaryType)
            {
            case PrimaryReleaseType::Album:
                res = Wt::WString::tr("Lms.Explore.Release.type-primary-album");
                break;
            case PrimaryReleaseType::Broadcast:
                res = Wt::WString::tr("Lms.Explore.Release.type-primary-broadcast");
                break;
            case PrimaryReleaseType::EP:
                res = Wt::WString::tr("Lms.Explore.Release.type-primary-ep");
                break;
            case PrimaryReleaseType::Single:
                res = Wt::WString::tr("Lms.Explore.Release.type-primary-single");
                break;
            case PrimaryReleaseType::Other:
                res = Wt::WString::tr("Lms.Explore.Release.type-primary-other");
                break;
            }
        }

        for (SecondaryReleaseType secondaryType : releaseType.secondaryTypes)
        {
            if (!res.empty())
                res += Wt::WString{ " · " };

            switch (secondaryType)
            {
            case SecondaryReleaseType::Compilation:
                res += Wt::WString::tr("Lms.Explore.Release.type-secondary-compilation");
                break;
            case SecondaryReleaseType::Spokenword:
                res += Wt::WString::tr("Lms.Explore.Release.type-secondary-spokenword");
                break;
            case SecondaryReleaseType::Soundtrack:
                res += Wt::WString::tr("Lms.Explore.Release.type-secondary-soundtrack");
                break;
            case SecondaryReleaseType::Interview:
                res += Wt::WString::tr("Lms.Explore.Release.type-secondary-interview");
                break;
            case SecondaryReleaseType::Audiobook:
                res += Wt::WString::tr("Lms.Explore.Release.type-secondary-audiobook");
                break;
            case SecondaryReleaseType::AudioDrama:
                res += Wt::WString::tr("Lms.Explore.Release.type-secondary-audiodrama");
                break;
            case SecondaryReleaseType::Live:
                res += Wt::WString::tr("Lms.Explore.Release.type-secondary-live");
                break;
            case SecondaryReleaseType::Remix:
                res += Wt::WString::tr("Lms.Explore.Release.type-secondary-remix");
                break;
            case SecondaryReleaseType::DJMix:
                res += Wt::WString::tr("Lms.Explore.Release.type-secondary-djmix");
                break;
            case SecondaryReleaseType::Mixtape_Street:
                res += Wt::WString::tr("Lms.Explore.Release.type-secondary-mixtape-street");
                break;
            case SecondaryReleaseType::Demo:
                res += Wt::WString::tr("Lms.Explore.Release.type-secondary-demo");
                break;
            case SecondaryReleaseType::FieldRecording:
                res += Wt::WString::tr("Lms.Explore.Release.type-secondary-field-recording");
                break;
            }
        }

        for (const std::string& customType : releaseType.customTypes)
        {
            if (!res.empty())
                res += Wt::WString{ " · " };

            res += customType;
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
} // namespace lms::ui::releaseHelpers
