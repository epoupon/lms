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
#include <string>
#include <string_view>

#include <Wt/Dbo/Field.h>
#include <Wt/Dbo/collection.h>

#include "database/IdRange.hpp"
#include "database/Object.hpp"
#include "database/Types.hpp"
#include "database/objects/ArtworkId.hpp"
#include "database/objects/MediumId.hpp"
#include "database/objects/ReleaseId.hpp"

namespace lms::db
{
    class Artwork;
    class Release;
    class Session;
    class Track;

    class Medium final : public Object<Medium, MediumId>
    {
    public:
        static const std::size_t maxMediaLength{ 64 };

        Medium() = default;

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

        static std::size_t getCount(Session& session);
        static pointer find(Session& session, MediumId id);
        static pointer find(Session& session, ReleaseId id, std::optional<std::size_t> position);
        static void find(Session& session, const IdRange<MediumId>& idRange, const std::function<void(const Medium::pointer&)>& func);
        static IdRange<MediumId> findNextIdRange(Session& session, MediumId lastRetrievedId, std::size_t count);
        static RangeResults<MediumId> findOrphanIds(Session& session, std::optional<Range> range = std::nullopt);

        // Updates
        static void updatePreferredArtwork(Session& session, MediumId mediumId, ArtworkId artworkId);

        // getters
        std::string_view getName() const { return _name; }
        std::optional<std::size_t> getPosition() const { return _position; }
        std::optional<std::size_t> getTrackCount() const { return _trackCount; } // not necessarily the number of tracks in the medium, but the number of tracks that should be in the medium
        std::string_view getMedia() const { return _media; }
        std::optional<float> getReplayGain() const { return _replayGain; }
        ReleaseId getReleaseId() const { return _release.id(); }
        ObjectPtr<Release> getRelease() const { return _release; }
        ObjectPtr<Artwork> getPreferredArtwork() const { return _preferredArtwork; }
        ArtworkId getPreferredArtworkId() const { return _preferredArtwork.id(); }

        // setters
        void setName(std::string_view name) { _name = name; }
        void setPosition(std::optional<std::size_t> position) { _position = position; }
        void setTrackCount(std::optional<std::size_t> trackCount) { _trackCount = trackCount; }
        void setMedia(std::string_view media) { _media = media; }
        void setReplayGain(std::optional<float> replayGain) { _replayGain = replayGain; }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _name, "name");
            Wt::Dbo::field(a, _position, "position");
            Wt::Dbo::field(a, _trackCount, "track_count");
            Wt::Dbo::field(a, _media, "media");
            Wt::Dbo::field(a, _replayGain, "replay_gain");

            Wt::Dbo::belongsTo(a, _release, "release", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::belongsTo(a, _preferredArtwork, "preferred_artwork", Wt::Dbo::OnDeleteSetNull);
            Wt::Dbo::hasMany(a, _tracks, Wt::Dbo::ManyToOne, "medium");
        }

    private:
        friend class Session;
        Medium(ObjectPtr<Release> release);
        static pointer create(Session& session, ObjectPtr<Release> release);

        std::string _name;
        std::optional<int> _position; // position in the release
        std::optional<int> _trackCount;
        std::string _media; // CD, etc.
        std::optional<float> _replayGain;

        Wt::Dbo::ptr<Release> _release;
        Wt::Dbo::ptr<Artwork> _preferredArtwork;
        Wt::Dbo::collection<Wt::Dbo::ptr<Track>> _tracks; // tracks that match this medium
    };
} // namespace lms::db
