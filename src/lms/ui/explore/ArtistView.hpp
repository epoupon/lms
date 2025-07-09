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

#include <map>

#include "database/Object.hpp"
#include "database/objects/ArtistId.hpp"
#include "database/objects/ArtworkId.hpp"
#include "database/objects/ReleaseId.hpp"

#include "ReleaseTypes.hpp"
#include "common/Template.hpp"

namespace lms::db
{
    class Artist;
    class Release;
} // namespace lms::db

namespace lms::ui
{
    class Filters;
    class PlayQueueController;
    class InfiniteScrollingContainer;

    class Artist : public Template
    {
    public:
        Artist(Filters& filters, PlayQueueController& controller);

    private:
        void refreshView();
        void refreshArtwork(db::ArtworkId artworkId);
        void refreshArtistInfo();
        void refreshReleases();
        void refreshAppearsOnReleases();
        void refreshNonReleaseTracks();
        void refreshSimilarArtists(const std::vector<db::ArtistId>& similarArtistsId);
        void refreshLinks(const db::ObjectPtr<db::Artist>& artist);

        struct ReleaseContainer;
        void addSomeReleases(ReleaseContainer& releaseContainer);
        bool addSomeNonReleaseTracks();
        static constexpr std::size_t _releasesBatchSize{ 6 };
        static constexpr std::size_t _tracksBatchSize{ 6 };
        static constexpr std::size_t _tracksMaxCount{ 160 };

        Filters& _filters;
        PlayQueueController& _playQueueController;

        // Display releases the same way as MusicBrainz
        struct ReleaseContainer
        {
            InfiniteScrollingContainer* container{};
            std::vector<db::ReleaseId> releases;
        };
        std::map<ReleaseType, ReleaseContainer> _releaseContainers;
        ReleaseContainer _appearsOnReleaseContainer{};
        InfiniteScrollingContainer* _trackContainer{};
        db::ArtistId _artistId{};
        bool _needForceRefresh{};
    };
} // namespace lms::ui
