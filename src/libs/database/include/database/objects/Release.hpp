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

#include <chrono>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <Wt/WDateTime.h>

#include "core/EnumSet.hpp"
#include "core/PartialDateTime.hpp"
#include "core/UUID.hpp"
#include "core/media/Codec.hpp"

#include "database/IdRange.hpp"
#include "database/Types.hpp"
#include "database/objects/ArtistId.hpp"
#include "database/objects/ArtworkId.hpp"
#include "database/objects/CountryId.hpp"
#include "database/objects/DirectoryId.hpp"
#include "database/objects/FilterExpr.hpp"
#include "database/objects/LabelId.hpp"
#include "database/objects/MediaLibraryId.hpp"
#include "database/objects/ReleaseId.hpp"
#include "database/objects/ReleaseTypeId.hpp"
#include "database/objects/Types.hpp"
#include "database/objects/UserId.hpp"
#include "database/objects/detail/Object.hpp"

namespace lms::db
{
    class ISession;

    class Country : public detail::Object<Country, CountryId>
    {
    public:
        static Country create(ISession& session, std::string_view name);
        static void destroy(ISession& session, std::span<const CountryId> ids);

        static std::size_t getCount(ISession& session);
        static std::optional<Country> find(ISession& session, CountryId id);
        static std::optional<Country> find(ISession& session, std::string_view name);
        static void find(ISession& session, ReleaseId release, const std::function<void(const Country&)>& func);
        static std::vector<CountryId> findOrphanIds(ISession& session, std::optional<Range> range = std::nullopt);

        // Accessors
        std::string_view getName() const { return _name; }

    protected:
        Country() = default;
        static constexpr std::size_t _maxNameLength{ 1024 };

        explicit Country(std::string_view name);

        std::string _name;
    };

    class Label : public detail::Object<Label, LabelId>
    {
    public:
        static Label create(ISession& session, std::string_view name);
        static void destroy(ISession& session, std::span<const LabelId> ids);

        static std::size_t getCount(ISession& session);
        static std::optional<Label> find(ISession& session, LabelId id);
        static std::optional<Label> find(ISession& session, std::string_view name);
        static void find(ISession& session, ReleaseId release, const std::function<void(const Label&)>& func);
        static void find(ISession& session, LabelSortMethod sortMethod, const std::function<void(const Label& label)>& func);
        static std::vector<LabelId> findOrphanIds(ISession& session, std::optional<Range> range = std::nullopt);

        // Accessors
        std::string_view getName() const { return _name; }

    protected:
        Label() = default;
        static constexpr std::size_t _maxNameLength{ 1024 };

        explicit Label(std::string_view name);

        std::string _name;
    };

    class ReleaseType : public detail::Object<ReleaseType, ReleaseTypeId>
    {
    public:
        static ReleaseType create(ISession& session, std::string_view name);
        static void destroy(ISession& session, std::span<const ReleaseTypeId> ids);

        static std::size_t getCount(ISession& session);
        static std::optional<ReleaseType> find(ISession& session, ReleaseTypeId id);
        static std::optional<ReleaseType> find(ISession& session, std::string_view name);
        static void find(ISession& session, ReleaseId release, const std::function<void(const ReleaseType&)>& func);
        static void find(ISession& session, ReleaseTypeSortMethod sortMethod, const std::function<void(const ReleaseType& releaseType)>& func);
        static std::vector<ReleaseTypeId> findOrphanIds(ISession& session, std::optional<Range> range = std::nullopt);

        // Accessors
        std::string_view getName() const { return _name; }

    protected:
        ReleaseType() = default;
        static constexpr std::size_t _maxNameLength{ 1024 };

        explicit ReleaseType(std::string_view name);

        std::string _name;
    };

    class Release : public detail::Object<Release, ReleaseId>
    {
    public:
        struct FindParameters
        {
            std::optional<FilterExpr> filter;                        // if set, releases must have a track matching this expression
            UserId filterUser;                                       // required if filter has per-user fields
            std::vector<std::string_view> keywords;                  // if non empty, name must match all of these keywords (cannot be set with keywords)
            std::string name;                                        // must match this name (cannot be set with keywords)
            ReleaseSortMethod sortMethod{ ReleaseSortMethod::None }; // the date sorts consider all the tracks of the release, not only the ones matching the filter
            std::optional<Range> range;
            UserId feedbackUser;                                     // if valid, only releases that have feedback from this user
            std::optional<FeedbackValue> feedbackValue;              // if set, only releases with feedback matching this value (any user, unless feedbackUser is also set)
            ArtistId artist;                                         // only releases by this release artist
            ArtistId trackArtist;                                    // only releases that involved this track artist
            core::EnumSet<TrackArtistLinkType> trackArtistLinkTypes; //    and for these link types, if set
            std::string releaseType;                                 // If set, releases that has this release type
            std::optional<core::UUID> releaseGroupMBID;              // If set, releases that belong to this release group
            DirectoryId directory;                                   // if set, releases in this directory (cannot be set with parent directory)
            DirectoryId parentDirectory;                             // if set, releases in this parent directory (cannot be set with directory)

            FindParameters& setFilter(std::optional<FilterExpr> _filter, UserId _user = {})
            {
                filter = std::move(_filter);
                filterUser = _user;
                return *this;
            }
            FindParameters& setKeywords(const std::vector<std::string_view>& _keywords)
            {
                keywords = _keywords;
                return *this;
            }
            FindParameters& setName(std::string_view _name)
            {
                name = _name;
                return *this;
            }
            FindParameters& setSortMethod(ReleaseSortMethod _sortMethod)
            {
                sortMethod = _sortMethod;
                return *this;
            }
            FindParameters& setRange(std::optional<Range> _range)
            {
                range = _range;
                return *this;
            }
            FindParameters& setFeedbackUser(UserId _user)
            {
                feedbackUser = _user;
                return *this;
            }
            FindParameters& setFeedbackValue(std::optional<FeedbackValue> _value)
            {
                feedbackValue = _value;
                return *this;
            }
            FindParameters& setArtist(ArtistId _artist)
            {
                artist = _artist;
                return *this;
            }
            FindParameters& setTrackArtist(ArtistId _trackArtist, core::EnumSet<TrackArtistLinkType> _trackArtistLinkTypes = {})
            {
                trackArtist = _trackArtist;
                trackArtistLinkTypes = _trackArtistLinkTypes;
                return *this;
            }
            FindParameters& setReleaseType(std::string_view _releaseType)
            {
                releaseType = _releaseType;
                return *this;
            }
            FindParameters& setReleaseGroupMBID(const std::optional<core::UUID>& _releaseGroupMBID)
            {
                releaseGroupMBID = _releaseGroupMBID;
                return *this;
            }
            FindParameters& setDirectory(DirectoryId _directory)
            {
                directory = _directory;
                return *this;
            }
            FindParameters& setParentDirectory(DirectoryId _parentDirectory)
            {
                parentDirectory = _parentDirectory;
                return *this;
            }
        };

        static Release create(ISession& session, const std::string& name, const std::optional<core::UUID>& MBID = {});
        static void destroy(ISession& session, std::span<const ReleaseId> ids);

        // Accessors
        static std::size_t getCount(ISession& session);
        static bool exists(ISession& session, ReleaseId id);
        static std::optional<Release> find(ISession& session, const core::UUID& MBID);
        static std::optional<Release> find(ISession& session, ReleaseId id);
        static void find(ISession& session, ReleaseId& lastRetrievedRelease, std::size_t count, const std::function<void(const Release&)>& func, MediaLibraryId library = {});
        static void find(ISession& session, const IdRange<ReleaseId>& idRange, const std::function<void(const Release&)>& func);
        static IdRange<ReleaseId> findNextIdRange(ISession& session, ReleaseId lastRetrievedId, std::size_t count);
        static std::vector<Release> find(ISession& session, const FindParameters& parameters);
        static void find(ISession& session, const FindParameters& parameters, const std::function<void(const Release&)>& func);
        static std::vector<ReleaseId> findIds(ISession& session, const FindParameters& parameters);
        static std::size_t getCount(ISession& session, const FindParameters& parameters);
        static std::vector<ReleaseId> findOrphanIds(ISession& session, std::optional<Range> range = std::nullopt); // not track related
        static bool hasArtist(ISession& session, ReleaseId id, ArtistId artistId);

        // Computed from the tracks (if all tracks have the same values, which is legit to not be the case)
        static core::PartialDateTime getDate(ISession& session, ReleaseId id);
        static std::optional<int> getYear(ISession& session, ReleaseId id);
        static core::PartialDateTime getOriginalDate(ISession& session, ReleaseId id);
        static std::optional<int> getOriginalYear(ISession& session, ReleaseId id);
        static bool hasVariousCopyrights(ISession& session, ReleaseId id);
        static std::optional<std::string> getCopyright(ISession& session, ReleaseId id);
        static std::optional<std::string> getCopyrightURL(ISession& session, ReleaseId id);
        static std::size_t getMeanBitrate(ISession& session, ReleaseId id);
        static std::vector<core::media::Codec> getCodecs(ISession& session, ReleaseId id);
        static std::chrono::milliseconds getDuration(ISession& session, ReleaseId id);
        static Wt::WDateTime getAddedTime(ISession& session, ReleaseId id);
        static Wt::WDateTime getLastWrittenTime(ISession& session, ReleaseId id);
        static std::size_t getTrackCount(ISession& session, ReleaseId id);
        static core::EnumSet<Advisory> getAdvisories(ISession& session, ReleaseId id);

        // Updates
        static void update(ISession& session, const Release& release);
        static void updatePreferredArtwork(ISession& session, ReleaseId id, ArtworkId artworkId);
        static void clearArtistLinks(ISession& session, ReleaseId id);
        static void clearLabels(ISession& session, ReleaseId id);
        static void clearCountries(ISession& session, ReleaseId id);
        static void clearReleaseTypes(ISession& session, ReleaseId id);
        static void addLabel(ISession& session, ReleaseId id, LabelId labelId);
        static void addCountry(ISession& session, ReleaseId id, CountryId countryId);
        static void addReleaseType(ISession& session, ReleaseId id, ReleaseTypeId releaseTypeId);

        // Accessors
        std::string_view getName() const { return _name; }
        std::string_view getSortName() const { return _sortName; }
        std::optional<core::UUID> getMBID() const { return _MBID; }
        std::optional<core::UUID> getGroupMBID() const { return _groupMBID; }
        std::optional<std::size_t> getTotalDisc() const { return _totalDisc; } // the number of discs this release should have if complete
        std::string_view getArtistDisplayName() const { return _artistDisplayName; }
        bool isCompilation() const { return _isCompilation; }
        std::string_view getBarcode() const { return _barcode; }
        std::string_view getComment() const { return _comment; }
        ArtworkId getPreferredArtworkId() const { return _preferredArtworkId; }

        // Setters
        void setName(std::string_view name) { _name = name; }
        void setSortName(std::string_view sortName) { _sortName = sortName; }
        void setMBID(const std::optional<core::UUID>& mbid) { _MBID = mbid; }
        void setGroupMBID(const std::optional<core::UUID>& mbid) { _groupMBID = mbid; }
        void setTotalDisc(std::optional<int> totalDisc) { _totalDisc = totalDisc; }
        void setArtistDisplayName(std::string_view name) { _artistDisplayName = name; }
        void setCompilation(bool value) { _isCompilation = value; }
        void setBarcode(std::string_view barcode) { _barcode = barcode; }
        void setComment(std::string_view comment) { _comment = comment; }

    protected:
        Release() = default;
        Release(const std::string& name, const std::optional<core::UUID>& MBID = {});

        static constexpr std::size_t _maxNameLength{ 512 };

        std::string _name;
        std::string _sortName;
        std::optional<core::UUID> _MBID;
        std::optional<core::UUID> _groupMBID;
        std::optional<int> _totalDisc{};
        std::string _artistDisplayName;
        bool _isCompilation{}; // See https://picard-docs.musicbrainz.org/en/appendices/tag_mapping.html#compilation-itunes-5
        std::string _barcode;
        std::string _comment;

        ArtworkId _preferredArtworkId;
    };

} // namespace lms::db
