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

#include "core/EnumSet.hpp"
#include "core/Utils.hpp"

#include "database/objects/Artist.hpp"
#include "database/objects/Release.hpp"

#include "Utils.hpp"
#include "explore/ReleaseTypes.hpp"

namespace lms::ui::releaseListHelpers
{
    namespace
    {
        enum class ReleaseOptions
        {
            ShowArtist,
            ShowYearAndOriginalYear,
            ShowYear,
        };

        std::unique_ptr<Wt::WTemplate> createEntryInternal(const db::Release::pointer& release, const db::Artist::pointer& artist, core::EnumSet<ReleaseOptions> options)
        {
            auto entry{ std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Releases.template.entry-grid")) };

            entry->bindWidget("release-name", utils::createReleaseAnchor(release));
            entry->addFunction("tr", &Wt::WTemplate::Functions::tr);

            {
                Wt::WAnchor* anchor{ entry->bindWidget("cover", utils::createReleaseAnchor(release, false)) };

                std::unique_ptr<Wt::WImage> image;
                if (release->getPreferredArtworkId().isValid())
                    image = utils::createArtworkImage(release->getPreferredArtworkId(), ArtworkResource::DefaultArtworkType::Release, ArtworkResource::Size::Large);
                else
                    image = utils::createDefaultArtworkImage(ArtworkResource::DefaultArtworkType::Release);

                image->addStyleClass("Lms-cover-release Lms-cover-anchor rounded"); // hack
                anchor->setImage(std::move(image));
            }

            if (options.contains(ReleaseOptions::ShowArtist))
            {
                auto artistAnchors{ utils::createArtistsAnchorsForRelease(release, artist ? artist->getId() : db::ArtistId{}, "link-secondary") };
                if (artistAnchors)
                {
                    entry->setCondition("if-has-artist", true);
                    entry->bindWidget("artist-name", std::move(artistAnchors));
                }
            }

            if (options.contains(ReleaseOptions::ShowYearAndOriginalYear))
            {
                Wt::WString year{ releaseHelpers::buildReleaseYearString(release->getYear(), release->getOriginalYear()) };
                if (!year.empty())
                {
                    entry->setCondition("if-has-year", true);
                    entry->bindString("year", year, Wt::TextFormat::Plain);
                }
            }
            else if (options.contains(ReleaseOptions::ShowYear))
            {
                if (release->getYear())
                {
                    entry->setCondition("if-has-year", true);
                    entry->bindInt("year", *release->getYear());
                }
            }

            return entry;
        }
    } // namespace

    std::unique_ptr<Wt::WTemplate> createEntry(const db::Release::pointer& release)
    {
        return createEntryInternal(release, db::Artist::pointer{}, core::EnumSet<ReleaseOptions>{ ReleaseOptions::ShowArtist });
    }

    std::unique_ptr<Wt::WTemplate> createEntryForArtist(const db::Release::pointer& release, const db::Artist::pointer& artist)
    {
        return createEntryInternal(release, artist, core::EnumSet<ReleaseOptions>{ ReleaseOptions::ShowArtist, ReleaseOptions::ShowYearAndOriginalYear });
    }

    std::unique_ptr<Wt::WTemplate> createEntryForOtherVersions(const db::ObjectPtr<db::Release>& release)
    {
        return createEntryInternal(release, db::Artist::pointer{}, core::EnumSet<ReleaseOptions>{ ReleaseOptions::ShowYear });
    }
} // namespace lms::ui::releaseListHelpers

namespace lms::ui::releaseHelpers
{
    namespace
    {
        static const Wt::WString releaseTypeSeparator{ " · " };

        Wt::WString buildPicardReleaseTypeString(const PicardReleaseType& releaseType)
        {
            Wt::WString res;

            switch (releaseType.primaryType)
            {
            case PicardReleaseType::PrimaryType::Album:
                res = Wt::WString::tr("Lms.Explore.Release.type-primary-album");
                break;
            case PicardReleaseType::PrimaryType::Broadcast:
                res = Wt::WString::tr("Lms.Explore.Release.type-primary-broadcast");
                break;
            case PicardReleaseType::PrimaryType::EP:
                res = Wt::WString::tr("Lms.Explore.Release.type-primary-ep");
                break;
            case PicardReleaseType::PrimaryType::Single:
                res = Wt::WString::tr("Lms.Explore.Release.type-primary-single");
                break;
            case PicardReleaseType::PrimaryType::Other:
                res = Wt::WString::tr("Lms.Explore.Release.type-primary-other");
                break;
            }

            for (PicardReleaseType::SecondaryType secondaryType : releaseType.secondaryTypes)
            {
                if (!res.empty())
                    res += releaseTypeSeparator;

                switch (secondaryType)
                {
                case PicardReleaseType::SecondaryType::Compilation:
                    res += Wt::WString::tr("Lms.Explore.Release.type-secondary-compilation");
                    break;
                case PicardReleaseType::SecondaryType::Spokenword:
                    res += Wt::WString::tr("Lms.Explore.Release.type-secondary-spokenword");
                    break;
                case PicardReleaseType::SecondaryType::Soundtrack:
                    res += Wt::WString::tr("Lms.Explore.Release.type-secondary-soundtrack");
                    break;
                case PicardReleaseType::SecondaryType::Interview:
                    res += Wt::WString::tr("Lms.Explore.Release.type-secondary-interview");
                    break;
                case PicardReleaseType::SecondaryType::Audiobook:
                    res += Wt::WString::tr("Lms.Explore.Release.type-secondary-audiobook");
                    break;
                case PicardReleaseType::SecondaryType::AudioDrama:
                    res += Wt::WString::tr("Lms.Explore.Release.type-secondary-audiodrama");
                    break;
                case PicardReleaseType::SecondaryType::Live:
                    res += Wt::WString::tr("Lms.Explore.Release.type-secondary-live");
                    break;
                case PicardReleaseType::SecondaryType::Remix:
                    res += Wt::WString::tr("Lms.Explore.Release.type-secondary-remix");
                    break;
                case PicardReleaseType::SecondaryType::DJMix:
                    res += Wt::WString::tr("Lms.Explore.Release.type-secondary-djmix");
                    break;
                case PicardReleaseType::SecondaryType::Mixtape_Street:
                    res += Wt::WString::tr("Lms.Explore.Release.type-secondary-mixtape-street");
                    break;
                case PicardReleaseType::SecondaryType::Demo:
                    res += Wt::WString::tr("Lms.Explore.Release.type-secondary-demo");
                    break;
                case PicardReleaseType::SecondaryType::FieldRecording:
                    res += Wt::WString::tr("Lms.Explore.Release.type-secondary-field-recording");
                    break;
                }
            }

            return res;
        }

        Wt::WString buildCustomReleaseTypeString(const CustomReleaseType& releaseType)
        {
            Wt::WString res;

            for (const std::string& customType : releaseType.types)
            {
                if (!res.empty())
                    res += releaseTypeSeparator;

                res += customType;
            }

            return res;
        }
    } // namespace

    Wt::WString buildReleaseTypeString(const ReleaseType& releaseType)
    {
        Wt::WString res;

        std::visit(core::utils::overloads{
                       [&](const PicardReleaseType& picardReleaseType) { res = buildPicardReleaseTypeString(picardReleaseType); },
                       [&](const CustomReleaseType& customReleaseType) { res = buildCustomReleaseTypeString(customReleaseType); } },
                   releaseType);

        return res;
    }

    Wt::WString buildReleaseYearString(std::optional<int> year, std::optional<int> originalYear)
    {
        Wt::WString res;

        // Year could be here, but originalYear can't be here without year (enforced by scanner)
        assert(year || !originalYear);

        if (!year)
            return res;

        if (originalYear && *originalYear != *year)
            res = std::to_string(*originalYear) + " (" + std::to_string(*year) + ")";
        else
            res = std::to_string(*year);

        return res;
    }
} // namespace lms::ui::releaseHelpers
