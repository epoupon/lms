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

#include <memory>
#include <optional>

#include "ReleaseTypes.hpp"
#include "database/Object.hpp"
#include <Wt/WString.h>
#include <Wt/WTemplate.h>
#include <resource/CoverResource.hpp>

namespace lms::ui
{
    class PlayQueueController;
    class Template;
}
namespace lms::db
{
    class ReleaseId;
    class Artist;
    class Release;
} // namespace lms::db

namespace lms::ui::releaseListHelpers
{
    void bindName(Template& entry, const db::ObjectPtr<db::Release>& release);
    void bindCover(Template& entry, const db::ObjectPtr<db::Release>& release, CoverResource::Size size);
    void bindArtists(Template& entry, const db::ObjectPtr<db::Release>& release, db::ArtistId omitIfMatchThisArtist = {}, std::string_view cssAnchorClass = "link-success");
    void bindDuration(Template& entry, const db::ObjectPtr<db::Release>& release);
    void bindPlayQueueController(Template& entry, const db::ObjectPtr<db::Release>& release, PlayQueueController& playQueueController, bool more);
    void bindStarred(Template& entry, const db::ObjectPtr<db::Release>& release);
    void bindDownload(Template& entry, const db::ObjectPtr<db::Release>& release);
    void bindInfo(Template& entry, const db::ObjectPtr<db::Release>& release);
    void bindMbid(Template& entry, const db::ObjectPtr<db::Release>& release);
    void bindReleaseYear(Template& entry, const db::ObjectPtr<db::Release>& release);

    std::unique_ptr<Wt::WTemplate> createEntry(const db::ObjectPtr<db::Release>& release);
    std::unique_ptr<Wt::WTemplate> createEntryForArtist(const db::ObjectPtr<db::Release>& release, const db::ObjectPtr<db::Artist>& artist);
} // namespace lms::ui::releaseListHelpers

namespace lms::ui::releaseHelpers
{
    Wt::WString buildReleaseTypeString(const ReleaseType& releaseType);
    Wt::WString buildReleaseYearString(std::optional<int> year, std::optional<int> originalYear);
    void showReleaseInfoModal(db::ReleaseId releaseId);
} // namespace lms::ui::releaseHelpers
