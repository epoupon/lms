/*
 * Copyright (C) 2024 Emeric Poupon
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

#include "TrackArtistLinkTypeSelector.hpp"

#include <Wt/WPushButton.h>

namespace lms::ui
{
    TrackArtistLinkTypeSelector::TrackArtistLinkTypeSelector(std::optional<db::TrackArtistLinkType> defaultLinkType)
        : Wt::WTemplate{ Wt::WString::tr("Lms.Explore.Artists.template.track-artist-link-type-selector") }
    {
        Wt::WText* linkTypeTxt{ bindNew<Wt::WText>("link-type") };

        auto bindMenuItem{ [this, linkTypeTxt, defaultLinkType](const std::string& var, const Wt::WString& title, std::optional<db::TrackArtistLinkType> linkType)
               {
                   auto* menuItem{ bindNew<Wt::WPushButton>(var, title) };
                   menuItem->clicked().connect([=]
                   {
                       _currentActiveItem->removeStyleClass("active");
                       menuItem->addStyleClass("active");
                       _currentActiveItem = menuItem;
                       linkTypeTxt->setText(title);

                       linkTypeChanged.emit(linkType);
                   });

                   if (linkType == defaultLinkType)
                   {
                       _currentActiveItem = menuItem;
                       _currentActiveItem->addStyleClass("active");
                       linkTypeTxt->setText(title);
                   }
               } };

        bindMenuItem("link-type-all", Wt::WString::tr("Lms.Explore.Artists.linktype-all"), std::nullopt);
        bindMenuItem("link-type-artist", Wt::WString::trn("Lms.Explore.Artists.linktype-artist", 2), db::TrackArtistLinkType::Artist);
        bindMenuItem("link-type-releaseartist", Wt::WString::trn("Lms.Explore.Artists.linktype-releaseartist", 2), db::TrackArtistLinkType::ReleaseArtist);
        bindMenuItem("link-type-composer", Wt::WString::trn("Lms.Explore.Artists.linktype-composer", 2), db::TrackArtistLinkType::Composer);
        bindMenuItem("link-type-conductor", Wt::WString::trn("Lms.Explore.Artists.linktype-conductor", 2), db::TrackArtistLinkType::Conductor);
        bindMenuItem("link-type-lyricist", Wt::WString::trn("Lms.Explore.Artists.linktype-lyricist", 2), db::TrackArtistLinkType::Lyricist);
        bindMenuItem("link-type-mixer", Wt::WString::trn("Lms.Explore.Artists.linktype-mixer", 2), db::TrackArtistLinkType::Mixer);
        bindMenuItem("link-type-performer", Wt::WString::trn("Lms.Explore.Artists.linktype-performer", 2), db::TrackArtistLinkType::Performer);
        bindMenuItem("link-type-producer", Wt::WString::trn("Lms.Explore.Artists.linktype-producer", 2), db::TrackArtistLinkType::Producer);
        bindMenuItem("link-type-remixer", Wt::WString::trn("Lms.Explore.Artists.linktype-remixer", 2), db::TrackArtistLinkType::Remixer);
    }
} // namespace lms::ui

