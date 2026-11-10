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
#include <string>
#include <string_view>
#include <vector>

#include <Wt/WTime.h>

#include "database/IdType.hpp"
#include "database/objects/Types.hpp"
#include "database/objects/detail/Object.hpp"

LMS_DECLARE_IDTYPE(ScanSettingsId)

namespace lms::db
{
    class ISession;

    class ScanSettings : public detail::Object<ScanSettings, ScanSettingsId>
    {
    public:
        // Do not modify values (just add)
        enum class UpdatePeriod
        {
            Never = 0,
            Daily,
            Weekly,
            Monthly,
            Hourly,
        };

        static ScanSettings create(ISession& session, std::string_view name = "");

        static void destroy(ISession& session, std::span<const ScanSettingsId> ids);

        static std::optional<ScanSettings> find(ISession& session, std::string_view name = "");
        static std::optional<ScanSettings> find(ISession& session, ScanSettingsId id);

        static void update(ISession& session, const ScanSettings& scanSettings);

        // Getters
        std::size_t getAudioScanVersion() const { return _audioScanVersion; }
        std::size_t getArtistInfoScanVersion() const { return _artistInfoScanVersion; }
        std::string_view getMusicNNModelIdentifier() const { return _musicnnModelIdentifier; }
        Wt::WTime getUpdateStartTime() const { return _startTime; }
        UpdatePeriod getUpdatePeriod() const { return _updatePeriod; }
        std::vector<std::string_view> getExtraTagsToScan() const;
        RecommendationEngineType getRecommendationEngineType() const { return _recommendationEngineType; }
        std::vector<std::string> getArtistTagDelimiters() const;
        std::vector<std::string> getDefaultTagDelimiters() const;
        std::vector<std::string> getArtistsToNotSplit() const;
        bool getSkipSingleReleasePlayLists() const { return _skipSingleReleasePlayLists; }
        bool getAllowMBIDArtistMerge() const { return _allowMBIDArtistMerge; }
        bool getArtistImageFallbackToRelease() const { return _artistImageFallbackToRelease; }

        // Setters
        void setUpdateStartTime(Wt::WTime t) { _startTime = t; }
        void setUpdatePeriod(UpdatePeriod p) { _updatePeriod = p; }
        void setExtraTagsToScan(std::span<const std::string_view> extraTags);
        void setRecommendationEngineType(RecommendationEngineType type) { _recommendationEngineType = type; }
        void setArtistTagDelimiters(std::span<const std::string_view> delimiters);
        void setArtistsToNotSplit(std::span<const std::string_view> artists);
        void setDefaultTagDelimiters(std::span<const std::string_view> delimiters);
        void setSkipSingleReleasePlayLists(bool value);
        void setAllowMBIDArtistMerge(bool value);
        void setArtistImageFallbackToRelease(bool value);
        void setMusicNNModelIdentifier(std::string_view identifier) { _musicnnModelIdentifier = identifier; }

    protected:
        ScanSettings() = default;
        explicit ScanSettings(std::string_view name);

        void incAudioScanVersion();

        std::string _name;
        int _audioScanVersion{};
        int _artistInfoScanVersion{};
        Wt::WTime _startTime = Wt::WTime{ 0, 0, 0 };
        UpdatePeriod _updatePeriod{ UpdatePeriod::Never };
        RecommendationEngineType _recommendationEngineType{ RecommendationEngineType::Clusters };
        std::string _extraTagsToScan;
        std::string _artistTagDelimiters;
        std::string _artistsToNotSplit;
        std::string _defaultTagDelimiters;
        bool _skipSingleReleasePlayLists{ true };
        bool _allowMBIDArtistMerge{ true };
        bool _artistImageFallbackToRelease{ true };
        std::string _musicnnModelIdentifier;
    };
} // namespace lms::db
