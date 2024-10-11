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

#include <chrono>
#include <string>
#include <string_view>

#include <Wt/WContainerWidget.h>
#include <Wt/WImage.h>
#include <Wt/WInteractWidget.h>
#include <Wt/WString.h>

#include "database/ArtistId.hpp"
#include "database/ClusterId.hpp"
#include "database/Object.hpp"
#include "database/ReleaseId.hpp"
#include "database/TrackId.hpp"

#include "resource/ArtworkResource.hpp"

namespace lms::db
{
    class Artist;
    class Cluster;
    class Release;
    class Track;
    class TrackList;
} // namespace lms::db

namespace lms::ui
{
    class Filters;
}

namespace lms::ui::utils
{
    std::string durationToString(std::chrono::milliseconds msDuration);

    std::unique_ptr<Wt::WImage> createReleaseCover(db::ReleaseId releaseId, ArtworkResource::Size size);
    std::unique_ptr<Wt::WImage> createTrackImage(db::TrackId trackId, ArtworkResource::Size size);
    std::unique_ptr<Wt::WImage> createArtistImage(db::ArtistId artistId, ArtworkResource::Size size);

    std::unique_ptr<Wt::WInteractWidget> createFilter(const Wt::WString& name, const Wt::WString& tooltip, std::string_view colorStyleClass, bool canDelete = false);
    std::unique_ptr<Wt::WInteractWidget> createFilterCluster(db::ClusterId clusterId, bool canDelete = false);
    std::unique_ptr<Wt::WContainerWidget> createFilterClustersForTrack(db::ObjectPtr<db::Track> track, Filters& filters);

    std::unique_ptr<Wt::WContainerWidget> createArtistAnchorList(const std::vector<db::ArtistId>& artistIds, std::string_view cssAnchorClass = "link-success");
    std::unique_ptr<Wt::WContainerWidget> createArtistDisplayNameWithAnchors(std::string_view displayName, const std::vector<db::ArtistId>& artistIds, std::string_view cssAnchorClass = "link-success");
    std::unique_ptr<Wt::WContainerWidget> createArtistsAnchorsForRelease(db::ObjectPtr<db::Release> release, db::ArtistId omitIfMatchThisArtist = {}, std::string_view cssAnchorClass = "link-success");

    Wt::WLink createArtistLink(db::ObjectPtr<db::Artist> artist);
    std::unique_ptr<Wt::WAnchor> createArtistAnchor(db::ObjectPtr<db::Artist> artist, bool setText = true);
    Wt::WLink createReleaseLink(db::ObjectPtr<db::Release> release);
    std::unique_ptr<Wt::WAnchor> createReleaseAnchor(db::ObjectPtr<db::Release> release, bool setText = true);
    std::unique_ptr<Wt::WAnchor> createTrackListAnchor(db::ObjectPtr<db::TrackList> trackList, bool setText = true);
} // namespace lms::ui::utils
