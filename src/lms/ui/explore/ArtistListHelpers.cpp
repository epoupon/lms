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

#include "Utils.hpp"

#include <Wt/WAnchor.h>
#include <common/Template.hpp>

namespace lms::ui::ArtistListHelpers
{
    void bindName(Template& entry, const db::ObjectPtr<db::Artist>& artist)
    {
        entry.bindWidget("name", utils::createArtistAnchor(artist));
    }

    void bindCover(Template& entry, const db::ObjectPtr<db::Artist>& artist)
    {
        Wt::WAnchor* anchor = entry.bindWidget("cover", utils::createArtistAnchor(artist, false));
        auto cover = utils::createCover(artist->getId(), CoverResource::Size::Small);
        cover->addStyleClass("Lms-cover-track Lms-cover-anchor"); // HACK
        anchor->setImage(std::move(cover));
    }

    std::unique_ptr<Wt::WTemplate> createEntry(const db::ObjectPtr<db::Artist>& artist)
    {
        auto entry = std::make_unique<Template>(Wt::WString::tr("Lms.Explore.Artists.template.entry"));

        bindName(*entry, artist);
        bindCover(*entry, artist);

        return entry;
    }
} // namespace lms::ui::ArtistListHelpers