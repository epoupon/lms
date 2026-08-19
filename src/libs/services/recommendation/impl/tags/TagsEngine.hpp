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

#include <cstdint>
#include <functional>
#include <span>
#include <unordered_map>
#include <vector>

#include "database/IdType.hpp"

#include "track-selection-constraints/TrackCandidateEvaluator.hpp"
#include "track-selection-constraints/TrackMetadata.hpp"

#include "IEngine.hpp"

namespace lms::db
{
    class Session;
}

namespace lms::recommendation
{
    struct TagId
    {
        enum class Type : std::uint8_t
        {
            Genre,
            Mood,
            Grouping,
            Language
        };
        Type type;
        db::IdType id;

        auto operator<=>(const TagId&) const = default;
    };
} // namespace lms::recommendation

namespace std
{
    template<>
    struct hash<lms::recommendation::TagId>
    {
        std::size_t operator()(const lms::recommendation::TagId& tagId) const noexcept
        {
            using UnderlyingType1 = std::underlying_type<lms::recommendation::TagId::Type>::type;
            using UnderlyingType2 = lms::db::IdType::ValueType;

            const std::size_t h1{ std::hash<UnderlyingType1>{}(static_cast<UnderlyingType1>(tagId.type)) };
            const std::size_t h2{ std::hash<UnderlyingType2>{}(tagId.id.getValue()) };
            return h1 ^ (h2 << 4);
        }
    };
} // namespace std

namespace lms::recommendation
{
    class TagsEngine : public IEngine
    {
    public:
        TagsEngine(db::IDb& db);
        ~TagsEngine() override;
        TagsEngine(const TagsEngine&) = delete;
        TagsEngine& operator=(const TagsEngine&) = delete;

    private:
        void load() override;

        TrackResults findSimilarTracksFromTrackList(db::TrackListId tracklistId, std::size_t maxCount) const override;
        TrackResults findSimilarTracks(std::span<const db::TrackId> trackIds, std::size_t maxCount) const override;
        TrackResults findTrackSimilarityPath(db::TrackId startTrackId, db::TrackId endTrackId, std::size_t maxCount) const override;
        ReleaseResults findSimilarReleases(db::ReleaseId releaseId, std::size_t maxCount) const override;
        ArtistResults findSimilarArtists(db::ArtistId artistId, core::EnumSet<db::TrackArtistLinkType> linkTypes, std::size_t maxCount) const override;

        TrackResults greedySelect(std::vector<db::TrackId> candidates, std::vector<db::TrackId> selectedTracks, std::size_t maxCount) const;
        void buildTrackTags(db::Session& session);
        void buildTrackMetadata(db::Session& session);
        void buildReleaseTags();
        void buildArtistTags();

        db::IDb& _db;

        TrackMetadataMap _trackMetadata;
        std::unordered_map<db::TrackId, std::vector<TagId>> _trackTags;
        std::unordered_map<db::ReleaseId, std::vector<TagId>> _releaseTags;
        std::unordered_map<db::ArtistId, std::vector<TagId>> _artistTags;
        TrackCandidateEvaluator _trackEvaluator;
    };
} // namespace lms::recommendation
