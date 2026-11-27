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

#include <optional>
#include <span>
#include <variant>
#include <vector>

#include "database/objects/ArtworkId.hpp"
#include "database/objects/Medium.hpp"
#include "database/objects/Movement.hpp"
#include "database/objects/ReleaseId.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/Work.hpp"

#include "common/Template.hpp"

namespace lms::db
{
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
        void refreshReleaseArtists(const db::Release& release);
        void refreshCopyright(const db::Release& release);
        void refreshLinks(const db::Release& release);
        void refreshOtherVersions(const db::Release& release);
        void refreshRelatedReleases(std::span<const db::ReleaseId> similarReleaseIds);

        struct TrackInfo
        {
            db::Track track;
            std::optional<db::Work> work;         // first work, if any
            std::optional<db::Movement> movement; // first movement, if any
        };

        using TrackInfoList = std::vector<TrackInfo>;
        struct WorkInfo
        {
            std::optional<db::Work> work;
            TrackInfoList tracks;
        };

        struct DiscInfo
        {
            db::Medium medium;
            using Segment = std::variant<WorkInfo, TrackInfoList>;
            std::vector<Segment> segments;
        };
        void refreshDiscs(const db::Release& release, std::span<const DiscInfo> discoInfoList);
        static std::vector<DiscInfo> createDiscInfoList(const db::Release& release);
        static DiscInfo createDiscInfo(const db::Medium& medium);

        static void appendTrackIds(std::vector<db::TrackId>& trackIds, const DiscInfo& disc);
        static void appendTrackIds(std::vector<db::TrackId>& trackIds, const TrackInfoList& tracks);

        struct DisplayOptions
        {
            bool displayTrackArtists{};
            bool showDiscHeaders{};
        };
        void addDisc(Wt::WContainerWidget* container, const DiscInfo& discInfo, DisplayOptions displayOptions);

        static TrackInfoList collectMediumTrackInfoList(const db::Medium& medium);

        Wt::WContainerWidget* addSegment(Wt::WContainerWidget* container, const Wt::WString& title, db::ArtworkId artworkId, std::span<const db::TrackId> trackIds, const char* segmentTemplate);
        void addTrackEntries(Wt::WContainerWidget* container, std::span<const TrackInfo> tracks, bool displayTrackArtists);
        void addTrackEntry(Wt::WContainerWidget* container, const TrackInfo& trackInfo, bool displayTrackArtists);

        Filters& _filters;
        PlayQueueController& _playQueueController;
        db::ReleaseId _releaseId;
        std::vector<db::TrackId> _trackIds; // ordered as displayed, spans all discs/works/movements
    };
} // namespace lms::ui
