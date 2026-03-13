/*
 * Copyright (C) 2026 Emeric Poupon
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

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <Wt/WContainerWidget.h>

#include "database/Object.hpp"
#include "database/objects/DirectoryId.hpp"
#include "database/objects/ReleaseId.hpp"

#include "common/Template.hpp"

namespace lms::db
{
    class Directory;
    class Release;
} // namespace lms::db

namespace lms::ui
{
    class Filters;
    class PlayQueueController;

    class Folders : public Template
    {
    public:
        Folders(Filters& filters, PlayQueueController& playQueueController);

    private:
        void onPathOrFilterChanged();
        void refreshView();

        void renderRootDirectories();
        void renderDirectory(db::DirectoryId directoryId);
        void renderBreadcrumbs(std::optional<db::DirectoryId> currentDirectoryId);
        void renderDirectories(const std::vector<db::ObjectPtr<db::Directory>>& directories, const std::unordered_map<db::DirectoryId::ValueType, db::ReleaseId>& directReleaseTargets);
        void renderReleases(const std::vector<db::ObjectPtr<db::Release>>& releases);
        bool hasContentFilters() const;

        std::optional<db::DirectoryId> getDirectoryFromPath() const;
        std::vector<db::ObjectPtr<db::Release>> getReleasesInDirectory(db::DirectoryId directoryId) const;

        bool isOnFoldersPath() const;

        static constexpr std::size_t _maxReleaseCount{ 1000 };

        Filters& _filters;
        PlayQueueController& _playQueueController;
        Wt::WContainerWidget* _breadcrumbs{};
        Wt::WContainerWidget* _directories{};
        Wt::WContainerWidget* _releases{};
        std::vector<db::ReleaseId> _currentReleaseIds;
    };
} // namespace lms::ui
