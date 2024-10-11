/*
 * Copyright (C) 2020 Emeric Poupon
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
#include "ArtistListHelpers.hpp"

#include "database/Artist.hpp"
#include "database/Session.hpp"

#include "LmsApplication.hpp"
#include "Utils.hpp"

namespace lms::ui::ArtistListHelpers
{
    std::unique_ptr<Wt::WTemplate> createEntry(const db::ObjectPtr<db::Artist>& artist)
    {
        auto entry{ std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Artists.template.entry")) };
        entry->bindWidget("name", utils::createArtistAnchor(artist));

        Wt::WAnchor* anchor{ entry->bindWidget("image", utils::createArtistAnchor(artist, false)) };
        auto image{ utils::createArtistImage(artist->getId(), ArtworkResource::Size::Large) };
        image->addStyleClass("Lms-cover-release Lms-cover-anchor");
        anchor->setImage(std::move(image));

        return entry;
    }
} // namespace lms::ui::ArtistListHelpers