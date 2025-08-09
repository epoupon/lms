/*
 * Copyright (C) 2018 Emeric Poupon
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

#include "database/Object.hpp"
#include "database/objects/ArtworkId.hpp"
#include "database/objects/ReleaseId.hpp"

#include "common/Template.hpp"

namespace lms::db
{
    class Medium;
    class Release;
} // namespace lms::db

namespace lms::ui
{
    class Filters;
    class PlayQueueController;

    class Release : public Template
    {
    public:
        Release(Filters& filters, PlayQueueController& playQueueController);

    private:
        void refreshView();
        void refreshArtwork(db::ArtworkId artworkId);
        void refreshReleaseArtists(const db::ObjectPtr<db::Release>& release);
        void refreshDiscs(const db::ObjectPtr<db::Release>& release);
        void createTracks(Wt::WContainerWidget* tracksContainer, const db::ObjectPtr<db::Medium>& medium, bool displayTrackArtists);
        void refreshCopyright(const db::ObjectPtr<db::Release>& release);
        void refreshLinks(const db::ObjectPtr<db::Release>& release);
        void refreshOtherVersions(const db::ObjectPtr<db::Release>& release);
        void refreshSimilarReleases(const std::vector<db::ReleaseId>& similarReleaseIds);

        std::unique_ptr<Wt::WTemplate> createDisc(const db::ObjectPtr<db::Medium>& medium);

        Filters& _filters;
        PlayQueueController& _playQueueController;
        db::ReleaseId _releaseId;
        bool _needForceRefresh{};
    };
} // namespace lms::ui
