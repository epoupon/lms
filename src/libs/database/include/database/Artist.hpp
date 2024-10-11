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

#include <Wt/Dbo/Dbo.h>
#include <Wt/WDateTime.h>

#include "core/EnumSet.hpp"
#include "core/UUID.hpp"
#include "database/ArtistId.hpp"
#include "database/ClusterId.hpp"
#include "database/MediaLibraryId.hpp"
#include "database/Object.hpp"
#include "database/ReleaseId.hpp"
#include "database/TrackId.hpp"
#include "database/Types.hpp"
#include "database/UserId.hpp"

namespace lms::db
{

    class Cluster;
    class ClusterType;
    class Image;
    class Release;
    class Session;
    class StarredArtist;
    class Track;
    class TrackArtistLink;
    class User;

    class Artist final : public Object<Artist, ArtistId>
    {
    public:
        struct FindParameters
        {
            std::vector<ClusterId> clusters;             // if non empty, at least one artist that belongs to these clusters
            std::vector<std::string_view> keywords;      // if non empty, name must match all of these keywords (on either name field OR sort name field)
            std::optional<TrackArtistLinkType> linkType; // if set, only artists that have produced at least one track with this link type
            ArtistSortMethod sortMethod{ ArtistSortMethod::None };
            std::optional<Range> range;
            Wt::WDateTime writtenAfter;
            UserId starringUser;                            // only artists starred by this user
            std::optional<FeedbackBackend> feedbackBackend; // and for this feedback backend
            TrackId track;                                  // artists involved in this track
            ReleaseId release;                              // artists involved in this release
            MediaLibraryId mediaLibrary;                    // artists that belong to this library

            FindParameters& setClusters(std::span<const ClusterId> _clusters)
            {
                clusters.assign(std::cbegin(_clusters), std::cend(_clusters));
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
            FindParameters& setMediaLibrary(MediaLibraryId _mediaLibrary)
            {
                mediaLibrary = _mediaLibrary;
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
        static RangeResults<pointer> find(Session& session, const FindParameters& parameters);
        static void find(Session& session, const FindParameters& parameters, std::function<void(const pointer&)> func);
        static RangeResults<ArtistId> findIds(Session& session, const FindParameters& parameters);
        static RangeResults<ArtistId> findOrphanIds(Session& session, std::optional<Range> range = std::nullopt); // No track related
        static bool exists(Session& session, ArtistId id);

        // Accessors
        const std::string& getName() const { return _name; }
        const std::string& getSortName() const { return _sortName; }
        std::optional<core::UUID> getMBID() const { return core::UUID::fromString(_MBID); }
        ObjectPtr<Image> getImage() const;

        // No artistLinkTypes means get them all
        RangeResults<ArtistId> findSimilarArtistIds(core::EnumSet<TrackArtistLinkType> artistLinkTypes = {}, std::optional<Range> range = std::nullopt) const;

        // Get the cluster of the tracks made by this artist
        // Each clusters are grouped by cluster type, sorted by the number of occurence
        // size is the max number of cluster per cluster type
        std::vector<std::vector<ObjectPtr<Cluster>>> getClusterGroups(std::vector<ClusterTypeId> clusterTypeIds, std::size_t size) const;

        void setName(std::string_view name);
        void setMBID(const std::optional<core::UUID>& mbid) { _MBID = mbid ? mbid->getAsString() : ""; }
        void setSortName(std::string_view sortName);
        void setImage(ObjectPtr<Image> image);

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _name, "name");
            Wt::Dbo::field(a, _sortName, "sort_name");
            Wt::Dbo::field(a, _MBID, "mbid");

            Wt::Dbo::belongsTo(a, _image, "image", Wt::Dbo::OnDeleteSetNull);
            Wt::Dbo::hasMany(a, _trackArtistLinks, Wt::Dbo::ManyToOne, "artist");
            Wt::Dbo::hasMany(a, _starredArtists, Wt::Dbo::ManyToMany, "user_starred_artists", "", Wt::Dbo::OnDeleteCascade);
        }

    private:
        static constexpr std::size_t _maxNameLength{ 512 };

        friend class Session;
        // Create
        Artist(const std::string& name, const std::optional<core::UUID>& MBID = {});
        static pointer create(Session& session, const std::string& name, const std::optional<core::UUID>& UUID = {});

        std::string _name;
        std::string _sortName;
        std::string _MBID; // Musicbrainz Identifier

        Wt::Dbo::ptr<Image> _image;
        Wt::Dbo::collection<Wt::Dbo::ptr<TrackArtistLink>> _trackArtistLinks; // Tracks involving this artist
        Wt::Dbo::collection<Wt::Dbo::ptr<StarredArtist>> _starredArtists;     // starred entries for this artist
    };

} // namespace lms::db
