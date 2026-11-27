/*
 * Copyright (C) 2025 Emeric Poupon
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

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "database/IdRange.hpp"
#include "database/Types.hpp"
#include "database/objects/ArtworkId.hpp"
#include "database/objects/MediumId.hpp"
#include "database/objects/ReleaseId.hpp"
#include "database/objects/Types.hpp"
#include "database/objects/detail/Object.hpp"

namespace lms::db
{
    class ISession;

    class Medium : public detail::Object<Medium, MediumId>
    {
    public:
        static const std::size_t maxMediaLength{ 64 };

        static Medium create(ISession& session, ReleaseId release);
        static void destroy(ISession& session, std::span<const MediumId> ids);

        // find
        struct FindParameters
        {
            ReleaseId release;
            MediumSortMethod sortMethod{ MediumSortMethod::None };
            std::optional<Range> range;

            FindParameters& setRelease(ReleaseId _release)
            {
                release = _release;
                return *this;
            }
            FindParameters& setSortMethod(MediumSortMethod _sortMethod)
            {
                sortMethod = _sortMethod;
                return *this;
            }
            FindParameters& setRange(std::optional<Range> _range)
            {
                range = _range;
                return *this;
            }
        };

        static std::size_t getCount(ISession& session);
        static std::optional<Medium> find(ISession& session, MediumId id);
        static std::optional<Medium> find(ISession& session, ReleaseId id, std::optional<std::size_t> position);
        static void find(ISession& session, const IdRange<MediumId>& idRange, const std::function<void(const Medium&)>& func);
        static void find(ISession& session, const FindParameters& params, const std::function<void(const Medium&)>& func);
        static IdRange<MediumId> findNextIdRange(ISession& session, MediumId lastRetrievedId, std::size_t count);
        static std::vector<MediumId> findOrphanIds(ISession& session, std::optional<Range> range = std::nullopt);

        // Updates
        static void update(ISession& session, const Medium& medium);
        static void updatePreferredArtwork(ISession& session, MediumId mediumId, ArtworkId artworkId);

        // getters
        std::string_view getName() const { return _name; }
        std::optional<std::size_t> getPosition() const { return _position; }
        std::optional<std::size_t> getTrackCount() const { return _trackCount; } // not necessarily the number of tracks in the medium, but the number of tracks that should be in the medium
        std::string_view getMedia() const { return _media; }
        std::optional<float> getReplayGain() const { return _replayGain; }
        ReleaseId getReleaseId() const { return _releaseId; }
        ArtworkId getPreferredArtworkId() const { return _preferredArtworkId; }

        // setters
        void setName(std::string_view name) { _name = name; }
        void setPosition(std::optional<std::size_t> position) { _position = position; }
        void setTrackCount(std::optional<std::size_t> trackCount) { _trackCount = trackCount; }
        void setMedia(std::string_view media) { _media = media; }
        void setReplayGain(std::optional<float> replayGain) { _replayGain = replayGain; }

    protected:
        Medium() = default;

        std::string _name;
        std::optional<int> _position; // position in the release
        std::optional<int> _trackCount;
        std::string _media; // CD, etc.
        std::optional<float> _replayGain;

        ReleaseId _releaseId;
        ArtworkId _preferredArtworkId;
    };
} // namespace lms::db
