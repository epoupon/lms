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

#include <string>
#include <string_view>

#include <Wt/Dbo/Field.h>

#include "database/IdType.hpp"
#include "database/Object.hpp"
#include "database/Types.hpp"
#include "database/objects/ArtistId.hpp"
#include "database/objects/ReleaseId.hpp"
#include "database/objects/Types.hpp"

LMS_DECLARE_IDTYPE(ReleaseArtistLinkId)

namespace lms::db
{
    class Artist;
    class Session;
    class Release;

    class ReleaseArtistLink final : public Object<ReleaseArtistLink, ReleaseArtistLinkId>
    {
    public:
        struct FindParameters
        {
            std::optional<Range> range;
            ArtistId artist;   // if set, links involved with this artist
            ReleaseId release; // if set, artists involved in this release
            std::optional<bool> mbidMatched;
            bool sortNameNotEmpty{}; // if set, only links whose artist sort name is not empty
            ReleaseArtistLinkSortMethod sortMethod{ ReleaseArtistLinkSortMethod::None };

            FindParameters& setRange(std::optional<Range> _range)
            {
                range = _range;
                return *this;
            }
            FindParameters& setArtist(ArtistId _artist)
            {
                artist = _artist;
                return *this;
            }
            FindParameters& setRelease(ReleaseId _release)
            {
                release = _release;
                return *this;
            }
            FindParameters& setMBIDMatched(std::optional<bool> _mbidMatched)
            {
                mbidMatched = _mbidMatched;
                return *this;
            }
            FindParameters& setSortNameNotEmpty(bool _sortNameNotEmpty)
            {
                sortNameNotEmpty = _sortNameNotEmpty;
                return *this;
            }
            FindParameters& setSortMethod(ReleaseArtistLinkSortMethod _method)
            {
                sortMethod = _method;
                return *this;
            }
        };

        ReleaseArtistLink() = default;
        ReleaseArtistLink(const ObjectPtr<Release>& release, const ObjectPtr<Artist>& artist, bool artistMBIDMatched);

        static pointer find(Session& session, ReleaseArtistLinkId linkId);
        static void find(Session& session, const FindParameters& params, std::function<void(const pointer&)> func);
        static std::size_t getCount(Session& session);

        static void findArtistNameNoLongerMatch(Session& session, std::optional<Range> range, const std::function<void(const pointer&)>& func);
        static void findWithArtistNameAmbiguity(Session& session, std::optional<Range> range, bool allowArtistMBIDFallback, const std::function<void(const pointer&)>& func);

        // accessors
        ObjectPtr<Release> getRelease() const { return _release; }
        ObjectPtr<Artist> getArtist() const { return _artist; }
        ArtistId getArtistId() const { return _artist.id(); }
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
            Wt::Dbo::field(a, _artistName, "artist_name");
            Wt::Dbo::field(a, _artistSortName, "artist_sort_name");
            Wt::Dbo::field(a, _artistMBIDMatched, "artist_mbid_matched");

            Wt::Dbo::belongsTo(a, _release, "release", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::belongsTo(a, _artist, "artist", Wt::Dbo::OnDeleteCascade);
        }

    private:
        friend class Session;
        static pointer create(Session& session, const ObjectPtr<Release>& release, const ObjectPtr<Artist>& artist, bool artistMBIDMatched);

        TrackArtistLinkType _type{ TrackArtistLinkType::Artist };
        std::string _subType;
        std::string _artistName;     // as it was in the tags
        std::string _artistSortName; // as it was in the tags
        bool _artistMBIDMatched{};

        Wt::Dbo::ptr<Release> _release;
        Wt::Dbo::ptr<Artist> _artist;
    };
} // namespace lms::db
