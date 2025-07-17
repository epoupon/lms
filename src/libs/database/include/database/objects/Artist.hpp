/*
 * Copyright (C) 2015 Emeric Poupon
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

#include <Wt/Dbo/Field.h>
#include <Wt/Dbo/collection.h>
#include <Wt/WDateTime.h>

#include "core/EnumSet.hpp"
#include "core/UUID.hpp"
#include "database/IdRange.hpp"
#include "database/Object.hpp"
#include "database/Types.hpp"
#include "database/objects/ArtistId.hpp"
#include "database/objects/ArtworkId.hpp"
#include "database/objects/ClusterId.hpp"
#include "database/objects/Filters.hpp"
#include "database/objects/MediaLibraryId.hpp"
#include "database/objects/ReleaseId.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/UserId.hpp"

namespace lms::db
{
    class Artwork;
    class Cluster;
    class ClusterType;
    class Release;
    class Session;
    class StarredArtist;
    class Track;
    class TrackArtistLink;
    class User;

    class Artist final : public Object<Artist, ArtistId>
    {
    public:
        static constexpr std::size_t maxNameLength{ 512 };

        struct FindParameters
        {
            Filters filters;
            std::vector<std::string_view> keywords;      // if non empty, name must match all of these keywords (on either name field OR sort name field)
            std::optional<TrackArtistLinkType> linkType; // if set, only artists that have produced at least one track with this link type
            ArtistSortMethod sortMethod{ ArtistSortMethod::None };
            std::optional<Range> range;
            Wt::WDateTime writtenAfter;
            UserId starringUser;                            // only artists starred by this user
            std::optional<FeedbackBackend> feedbackBackend; // and for this feedback backend
            TrackId track;                                  // artists involved in this track
            ReleaseId release;                              // artists involved in this release

            FindParameters& setFilters(const Filters& _filters)
            {
                filters = _filters;
                return *this;
            }
            FindParameters& setKeywords(const std::vector<std::string_view>& _keywords)
            {
                keywords = _keywords;
                return *this;
            }
            FindParameters& setLinkType(std::optional<TrackArtistLinkType> _linkType)
            {
                linkType = _linkType;
                return *this;
            }
            FindParameters& setSortMethod(ArtistSortMethod _sortMethod)
            {
                sortMethod = _sortMethod;
                return *this;
            }
            FindParameters& setRange(std::optional<Range> _range)
            {
                range = _range;
                return *this;
            }
            FindParameters& setWrittenAfter(const Wt::WDateTime& _after)
            {
                writtenAfter = _after;
                return *this;
            }
            FindParameters& setStarringUser(UserId _user, FeedbackBackend _feedbackBackend)
            {
                starringUser = _user;
                feedbackBackend = _feedbackBackend;
                return *this;
            }
            FindParameters& setTrack(TrackId _track)
            {
                track = _track;
                return *this;
            }
            FindParameters& setRelease(ReleaseId _release)
            {
                release = _release;
                return *this;
            }
        };

        Artist() = default;

        // Accessors
        static std::size_t getCount(Session& session);
        static pointer find(Session& session, const core::UUID& MBID);
        static pointer find(Session& session, ArtistId id);
        static std::vector<pointer> find(Session& session, std::string_view name); // exact match on name field
        static void find(Session& session, ArtistId& lastRetrievedArtist, std::size_t count, const std::function<void(const Artist::pointer&)>& func, MediaLibraryId library = {});
        static void find(Session& session, const IdRange<ArtistId>& idRange, const std::function<void(const Artist::pointer&)>& func);
        static RangeResults<pointer> find(Session& session, const FindParameters& params);
        static void find(Session& session, const FindParameters& params, std::function<void(const pointer&)> func);
        static IdRange<ArtistId> findNextIdRange(Session& session, ArtistId lastRetrievedId, std::size_t count);
        static RangeResults<ArtistId> findIds(Session& session, const FindParameters& params);
        static RangeResults<ArtistId> findOrphanIds(Session& session, std::optional<Range> range = std::nullopt); // No track related
        static bool exists(Session& session, ArtistId id);

        // Updates
        static void updatePreferredArtwork(Session& session, ArtistId artistId, ArtworkId artworkId);

        // Accessors
        const std::string& getName() const { return _name; }
        const std::string& getSortName() const { return _sortName; }
        std::optional<core::UUID> getMBID() const;
        bool hasMBID() const;
        ObjectPtr<Artwork> getPreferredArtwork() const;
        ArtworkId getPreferredArtworkId() const;
        void visitLinks(std::function<void(const ObjectPtr<TrackArtistLink>& link)> visitor) const;

        // No artistLinkTypes means get them all
        RangeResults<ArtistId> findSimilarArtistIds(core::EnumSet<TrackArtistLinkType> artistLinkTypes = {}, std::optional<Range> range = std::nullopt) const;

        // Get the cluster of the tracks made by this artist
        // Each clusters are grouped by cluster type, sorted by the number of occurence
        // size is the max number of cluster per cluster type
        std::vector<std::vector<ObjectPtr<Cluster>>> getClusterGroups(std::span<const ClusterTypeId> clusterTypeIds, std::size_t size) const;

        void setName(std::string_view name);
        void setMBID(const std::optional<core::UUID>& mbid) { _mbid = mbid ? mbid->getAsString() : ""; }
        void setSortName(std::string_view sortName);
        void setPreferredArtwork(ObjectPtr<Artwork> artwork);

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _name, "name");
            Wt::Dbo::field(a, _sortName, "sort_name");
            Wt::Dbo::field(a, _mbid, "mbid");

            Wt::Dbo::belongsTo(a, _preferredArtwork, "preferred_artwork", Wt::Dbo::OnDeleteSetNull);
            Wt::Dbo::hasMany(a, _trackArtistLinks, Wt::Dbo::ManyToOne, "artist");
            Wt::Dbo::hasMany(a, _starredArtists, Wt::Dbo::ManyToMany, "user_starred_artists", "", Wt::Dbo::OnDeleteCascade);
        }

    private:
        friend class Session;
        // Create
        Artist(const std::string& name, const std::optional<core::UUID>& MBID = {});
        static pointer create(Session& session, const std::string& name, const std::optional<core::UUID>& mbid = std::nullopt);

        std::string _name;
        std::string _sortName;
        std::string _mbid; // Musicbrainz Identifier

        Wt::Dbo::ptr<Artwork> _preferredArtwork;
        Wt::Dbo::collection<Wt::Dbo::ptr<TrackArtistLink>> _trackArtistLinks; // Tracks involving this artist
        Wt::Dbo::collection<Wt::Dbo::ptr<StarredArtist>> _starredArtists;     // starred entries for this artist
    };

} // namespace lms::db
