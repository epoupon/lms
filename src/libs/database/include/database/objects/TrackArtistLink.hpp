/*
 * Copyright (C) 2013-2016 Emeric Poupon
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
#include <string>
#include <string_view>

#include <Wt/Dbo/Field.h>

#include "core/EnumSet.hpp"

#include "database/IdType.hpp"
#include "database/Object.hpp"
#include "database/Types.hpp"
#include "database/objects/ArtistId.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/Types.hpp"

LMS_DECLARE_IDTYPE(TrackArtistLinkId)

namespace lms::db
{
    class Artist;
    class ISession;
    class Track;

    class TrackArtistLink final : public Object<TrackArtistLink, TrackArtistLinkId>
    {
    public:
        TrackArtistLink() = default;

        static pointer create(ISession& session, const ObjectPtr<Track>& track, const ObjectPtr<Artist>& artist, TrackArtistLinkType type, std::string_view subType, bool artistMBIDMatched = false);
        static pointer create(ISession& session, const ObjectPtr<Track>& track, const ObjectPtr<Artist>& artist, TrackArtistLinkType type, bool artistMBIDMatched = false);
        TrackArtistLink(const ObjectPtr<Track>& track, const ObjectPtr<Artist>& artist, TrackArtistLinkType type, std::string_view subType, bool artistMBIDMatched);

        static void find(ISession& session, TrackId trackId, const std::function<void(const pointer&, const ObjectPtr<Artist>&)>& func);
        static pointer find(ISession& session, TrackArtistLinkId linkId);
        static std::size_t getCount(ISession& session);
        static core::EnumSet<TrackArtistLinkType> findUsedTypes(ISession& session, ArtistId _artist);
        static void findArtistNameNoLongerMatch(ISession& session, std::optional<Range> range, const std::function<void(const pointer&)>& func);
        static void findWithArtistNameAmbiguity(ISession& session, std::optional<Range> range, bool allowArtistMBIDFallback, const std::function<void(const pointer&)>& func);

        // accessors
        ObjectPtr<Track> getTrack() const { return _track; }
        ObjectPtr<Artist> getArtist() const { return _artist; }
        ArtistId getArtistId() const { return _artist.id(); }
        TrackArtistLinkType getType() const { return _type; }
        std::string_view getSubType() const { return _subType; }
        std::string_view getArtistName() const { return _artistName; }
        std::string_view getArtistSortName() const { return _artistSortName; }
        bool isArtistMBIDMatched() const { return _artistMBIDMatched; }

        // setters
        void setArtist(ObjectPtr<Artist> artist);
        void setArtistName(std::string_view artistName);
        void setArtistSortName(std::string_view artistSortName);

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _type, "type");
            Wt::Dbo::field(a, _subType, "subtype");
            Wt::Dbo::field(a, _artistName, "artist_name");
            Wt::Dbo::field(a, _artistSortName, "artist_sort_name");
            Wt::Dbo::field(a, _artistMBIDMatched, "artist_mbid_matched");

            Wt::Dbo::belongsTo(a, _track, "track", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::belongsTo(a, _artist, "artist", Wt::Dbo::OnDeleteCascade);
        }

    private:
        TrackArtistLinkType _type{ TrackArtistLinkType::Artist };
        std::string _subType;
        std::string _artistName;     // as it was in the tags
        std::string _artistSortName; // as it was in the tags
        bool _artistMBIDMatched{};

        Wt::Dbo::ptr<Track> _track;
        Wt::Dbo::ptr<Artist> _artist;
    };
} // namespace lms::db
