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
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <Wt/WContainerWidget.h>
#include <Wt/WImage.h>
#include <Wt/WInteractWidget.h>
#include <Wt/WString.h>

#include "core/EnumSet.hpp"

#include "audio/TranscodeTypes.hpp"

#include "database/objects/Artist.hpp"
#include "database/objects/Types.hpp"

#include "explore/Filters.hpp"
#include "resource/ArtworkResource.hpp"

namespace lms::db
{
    class Release;
    class Track;
    class TrackList;
} // namespace lms::db

namespace lms::ui::utils
{
    std::string durationToString(std::chrono::milliseconds msDuration);

    void showArtworkModal(Wt::WLink image);

    std::unique_ptr<Wt::WImage> createArtworkImage(db::ArtworkId artworkId, ArtworkResource::DefaultArtworkType type, ArtworkResource::Size size);
    std::unique_ptr<Wt::WImage> createDefaultArtworkImage(ArtworkResource::DefaultArtworkType type);

    // nullptr if the value no longer exists
    std::unique_ptr<Wt::WInteractWidget> createFilterValue(const Filters::Value& value);
    // nullptr if one of the values no longer exists
    std::unique_ptr<Wt::WInteractWidget> createFilter(std::span<const Filters::Value> values, Filters::Match match);
    std::unique_ptr<Wt::WContainerWidget> createFilterClustersForTrack(const db::Track& track, Filters& filters);

    std::unique_ptr<Wt::WAnchor> createArtistAnchor(const db::Artist& artist, bool setText = true);
    Wt::WLink createReleaseLink(const db::Release& release);
    std::unique_ptr<Wt::WAnchor> createReleaseAnchor(const db::Release& release, bool setText = true);
    std::unique_ptr<Wt::WAnchor> createTrackListAnchor(const db::TrackList& trackList, bool setText = true);

    std::unique_ptr<Wt::WContainerWidget> createArtistAnchorList(const std::vector<db::Artist>& artists, std::string_view cssAnchorClass = "link-success");

    struct ArtistDisplayInfo
    {
        std::string displayName;
        struct Entry
        {
            std::string displayName;
            db::Artist artist;
        };
        std::vector<Entry> entries;
    };
    ArtistDisplayInfo computeArtistDisplayInfo(const db::Release& release);
    ArtistDisplayInfo computeArtistDisplayInfo(const db::Track& track, db::TrackArtistLinkType linkType);

    std::unique_ptr<Wt::WContainerWidget> createArtistsAnchors(const ArtistDisplayInfo& artistDisplayInfo, std::string_view cssAnchorClass = "link-success");
    std::unique_ptr<Wt::WContainerWidget> createArtistsAnchors(const db::Track& track, db::TrackArtistLinkType linkType, std::string_view cssAnchorClass = "link-success");
    std::unique_ptr<Wt::WContainerWidget> createArtistsAnchors(const db::Release& release, std::string_view cssAnchorClass = "link-success");

    std::map<Wt::WString, std::vector<db::Artist>> getArtistsByRole(const db::Track& track, core::EnumSet<db::TrackArtistLinkType> artistLinkTypes = {});
    std::map<Wt::WString, std::vector<db::Artist>> getTrackArtistsByRole(const db::Release& release);

    std::unique_ptr<Wt::WInteractWidget> createCopyright(std::string_view copyright, std::string_view copyrightURL);

    void copyToClipboard(std::string_view text);

    struct TrackDisplayInfo
    {
        std::string title;                   // "i. MovementName" when track has work + movement, otherwise just the track's title
        std::optional<std::string> workName; // only set when title comes from a movement
    };
    TrackDisplayInfo computeTrackDisplayInfo(const db::Track& track);

    std::optional<audio::TranscodeOutputFormat> toSupportedTranscodeOutputFormat(db::TranscodingOutputFormat format);
} // namespace lms::ui::utils
