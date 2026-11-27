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

#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "database/IdType.hpp"
#include "database/Types.hpp"
#include "database/objects/ArtistId.hpp"
#include "database/objects/ReleaseId.hpp"
#include "database/objects/Types.hpp"
#include "database/objects/detail/Object.hpp"

LMS_DECLARE_IDTYPE(ReleaseArtistLinkId)

namespace lms::db
{
    class Artist;
    class ISession;

    class ReleaseArtistLink : public detail::Object<ReleaseArtistLink, ReleaseArtistLinkId>
    {
    public:
        static ReleaseArtistLink create(ISession& session, ReleaseId release, ArtistId artist, bool artistMBIDMatched);
        static void destroy(ISession& session, std::span<const ReleaseArtistLinkId> ids);

        static std::optional<ReleaseArtistLink> find(ISession& session, ReleaseArtistLinkId linkId);
        static void find(ISession& session, ReleaseId release, const std::function<void(const ReleaseArtistLink&)>& func);
        static std::size_t getCount(ISession& session);

        static void findArtistNameNoLongerMatch(ISession& session, std::optional<Range> range, const std::function<void(const ReleaseArtistLink&)>& func);
        static void findWithArtistNameAmbiguity(ISession& session, std::optional<Range> range, bool allowArtistMBIDFallback, const std::function<void(const ReleaseArtistLink&)>& func);

        static void update(ISession& session, const ReleaseArtistLink& releaseArtistLink);

        // accessors
        ReleaseId getReleaseId() const { return _releaseId; }
        ArtistId getArtistId() const { return _artistId; }
        std::string_view getArtistName() const { return _artistName; }
        std::string_view getArtistSortName() const { return _artistSortName; }
        bool isArtistMBIDMatched() const { return _artistMBIDMatched; }

        // setters
        void setArtist(ArtistId artist) { _artistId = artist; }
        void setArtistName(std::string_view artistName);
        void setArtistSortName(std::string_view artistSortName);

    protected:
        ReleaseArtistLink() = default;
        explicit ReleaseArtistLink(bool artistMBIDMatched);

        TrackArtistLinkType _type{ TrackArtistLinkType::Artist };
        std::string _subType;
        std::string _artistName;     // as it was in the tags
        std::string _artistSortName; // as it was in the tags
        bool _artistMBIDMatched{};
        ReleaseId _releaseId;
        ArtistId _artistId;
    };
} // namespace lms::db
