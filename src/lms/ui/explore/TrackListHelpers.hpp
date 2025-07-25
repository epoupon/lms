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

#pragma once

#include <map>
#include <memory>
#include <set>

#include <Wt/WString.h>
#include <Wt/WWidget.h>

#include "core/EnumSet.hpp"
#include "database/Object.hpp"
#include "database/Types.hpp"
#include "database/objects/ArtistId.hpp"
#include "database/objects/TrackId.hpp"

namespace lms::db
{
    class Track;
}

namespace lms::ui
{
    class PlayQueueController;
    class Filters;
} // namespace lms::ui

namespace lms::ui::TrackListHelpers
{
    inline constexpr core::EnumSet<db::TrackArtistLinkType> AllArtistRoles{
        db::TrackArtistLinkType::Composer,
        db::TrackArtistLinkType::Conductor,
        db::TrackArtistLinkType::Lyricist,
        db::TrackArtistLinkType::Mixer,
        db::TrackArtistLinkType::Remixer,
        db::TrackArtistLinkType::Performer,
        db::TrackArtistLinkType::Producer,
    };

    std::map<Wt::WString, std::set<db::ArtistId>> getArtistsByRole(db::TrackId trackId, core::EnumSet<db::TrackArtistLinkType> artistLinkTypes = AllArtistRoles);
    void showTrackInfoModal(db::TrackId trackId, Filters& filters);
    void showTrackLyricsModal(db::TrackId trackId);
    std::unique_ptr<Wt::WWidget> createEntry(const db::ObjectPtr<db::Track>& track, PlayQueueController& playQueueController, Filters& filters);
} // namespace lms::ui::TrackListHelpers
