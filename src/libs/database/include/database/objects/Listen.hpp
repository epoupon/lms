/*
 * Copyright (C) 2021 Emeric Poupon
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

#include <Wt/WDateTime.h>

#include "database/Types.hpp"
#include "database/objects/ArtistId.hpp"
#include "database/objects/FilterExpr.hpp"
#include "database/objects/ListenId.hpp"
#include "database/objects/ReleaseId.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/Types.hpp"
#include "database/objects/UserId.hpp"
#include "database/objects/detail/Object.hpp"

namespace lms::db
{
    class Artist;
    class ISession;
    class Release;
    class Track;

    class Listen : public detail::Object<Listen, ListenId>
    {
    public:
        static Listen create(ISession& session, UserId user, TrackId track, const Wt::WDateTime& dateTime);
        static void destroy(ISession& session, std::span<const ListenId> ids);

        struct FindParameters
        {
            UserId user;
            std::optional<Range> range;

            FindParameters& setUser(UserId _user)
            {
                user = _user;
                return *this;
            }
            FindParameters& setRange(Range _range)
            {
                range = _range;
                return *this;
            }
        };

        // Accessors
        static std::size_t getCount(ISession& session);
        static std::optional<Listen> find(ISession& session, ListenId id);
        static std::optional<Listen> find(ISession& session, UserId userId, TrackId trackId, const Wt::WDateTime& dateTime);
        static std::vector<ListenId> find(ISession& session, const FindParameters& parameters);

        // Stats
        struct StatsFindParameters
        {
            UserId user;
            std::optional<FilterExpr> filter;       // if set, the listened tracks must match this expression
            UserId filterUser;                      // required if filter has per-user fields
            std::vector<std::string_view> keywords; // if non empty, name must match all of these keywords
            std::optional<Range> range;
            ArtistId artist; // if set, matching this artist

            StatsFindParameters& setUser(UserId _user)
            {
                user = _user;
                return *this;
            }
            StatsFindParameters& setFilter(std::optional<FilterExpr> _filter, UserId _user = {})
            {
                filter = std::move(_filter);
                filterUser = _user;
                return *this;
            }
            StatsFindParameters& setKeywords(const std::vector<std::string_view>& _keywords)
            {
                keywords = _keywords;
                return *this;
            }
            StatsFindParameters& setRange(std::optional<Range> _range)
            {
                range = _range;
                return *this;
            }
            StatsFindParameters& setArtist(ArtistId _artist)
            {
                artist = _artist;
                return *this;
            }
        };

        struct ArtistStatsFindParameters : public StatsFindParameters
        {
            std::optional<TrackArtistLinkType> trackArtistLinkType; // if set, only artists that have produced at least one track with this link type
            bool releaseArtistsOnly{};

            ArtistStatsFindParameters& setReleaseArtistsOnly(bool _releaseArtistsOnly)
            {
                releaseArtistsOnly = _releaseArtistsOnly;
                return *this;
            }
            ArtistStatsFindParameters& setTrackArtistLinkType(std::optional<TrackArtistLinkType> _trackArtistLinkType)
            {
                trackArtistLinkType = _trackArtistLinkType;
                return *this;
            }
        };

        static std::vector<ArtistId> getTopArtists(ISession& session, const ArtistStatsFindParameters& params);
        static std::vector<ReleaseId> getTopReleases(ISession& session, const StatsFindParameters& params);
        static std::vector<TrackId> getTopTracks(ISession& session, const StatsFindParameters& params);
        static std::vector<ArtistId> getRecentArtists(ISession& session, const ArtistStatsFindParameters& params);
        static std::vector<ReleaseId> getRecentReleases(ISession& session, const StatsFindParameters& params);
        static std::vector<TrackId> getRecentTracks(ISession& session, const StatsFindParameters& params);

        static void getTopArtists(ISession& session, const ArtistStatsFindParameters& params, const std::function<void(const Artist&)>& func);
        static void getTopReleases(ISession& session, const StatsFindParameters& params, const std::function<void(const Release&)>& func);
        static void getTopTracks(ISession& session, const StatsFindParameters& params, const std::function<void(const Track&)>& func);
        static void getRecentArtists(ISession& session, const ArtistStatsFindParameters& params, const std::function<void(const Artist&)>& func);
        static void getRecentReleases(ISession& session, const StatsFindParameters& params, const std::function<void(const Release&)>& func);
        static void getRecentTracks(ISession& session, const StatsFindParameters& params, const std::function<void(const Track&)>& func);

        static std::size_t getCount(ISession& session, UserId userId, TrackId trackId);
        static std::size_t getCount(ISession& session, UserId userId, ReleaseId trackId);

        static std::optional<Listen> getMostRecentListen(ISession& session, UserId userId, ReleaseId releaseId);
        static std::optional<Listen> getMostRecentListen(ISession& session, UserId userId, TrackId trackId);

        UserId getUserId() const { return _userId; }
        TrackId getTrackId() const { return _trackId; }
        const Wt::WDateTime& getDateTime() const { return _dateTime; }

    protected:
        Listen() = default;
        explicit Listen(const Wt::WDateTime& dateTime);

        Wt::WDateTime _dateTime;
        UserId _userId;
        TrackId _trackId;
    };

} // namespace lms::db
