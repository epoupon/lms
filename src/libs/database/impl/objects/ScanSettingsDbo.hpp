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

#include <Wt/Dbo/Dbo.h>
#include <Wt/Dbo/Field.h>

#include "database/objects/ScanSettings.hpp"

#include "DboAccess.hpp"

namespace lms::db::dbo
{
    class ScanSettings final : public db::ScanSettings, public Wt::Dbo::Dbo<ScanSettings>
    {
    public:
        ScanSettings() = default;
        using db::ScanSettings::ScanSettings;

        void syncIds()
        {
            _id = id();
        }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _name, "name");
            Wt::Dbo::field(a, _audioScanVersion, "audio_scan_version");
            Wt::Dbo::field(a, _artistInfoScanVersion, "artist_info_scan_version");
            Wt::Dbo::field(a, _startTime, "start_time");
            Wt::Dbo::field(a, _updatePeriod, "update_period");
            Wt::Dbo::field(a, _recommendationEngineType, "recommendation_engine_type");
            Wt::Dbo::field(a, _extraTagsToScan, "extra_tags_to_scan");
            Wt::Dbo::field(a, _artistTagDelimiters, "artist_tag_delimiters");
            Wt::Dbo::field(a, _artistsToNotSplit, "artists_to_not_split");
            Wt::Dbo::field(a, _defaultTagDelimiters, "default_tag_delimiters");
            Wt::Dbo::field(a, _skipSingleReleasePlayLists, "skip_single_release_playlists");
            Wt::Dbo::field(a, _allowMBIDArtistMerge, "allow_mbid_artist_merge");
            Wt::Dbo::field(a, _artistImageFallbackToRelease, "artist_image_fallback_to_release");
            Wt::Dbo::field(a, _musicnnModelIdentifier, "musicnn_model_identifier");
        }
    };
} // namespace lms::db::dbo

namespace lms::db
{
    template<>
    struct DboOf<ScanSettings>
    {
        using type = dbo::ScanSettings;
    };
} // namespace lms::db
