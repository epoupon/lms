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

#include <chrono>
#include <filesystem>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Wt/Dbo/Dbo.h>
#include <Wt/Dbo/WtSqlTraits.h>
#include <Wt/WDateTime.h>

#include "core/EnumSet.hpp"
#include "core/UUID.hpp"
#include "database/ArtistId.hpp"
#include "database/ClusterId.hpp"
#include "database/DirectoryId.hpp"
#include "database/MediaLibraryId.hpp"
#include "database/Object.hpp"
#include "database/ReleaseId.hpp"
#include "database/TrackId.hpp"
#include "database/TrackListId.hpp"
#include "database/Types.hpp"
#include "database/UserId.hpp"

namespace lms::db
{
    class Artist;
    class Cluster;
    class ClusterType;
    class Directory;
    class MediaLibrary;
    class Release;
    class Session;
    class TrackArtistLink;
    class TrackLyrics;
    class TrackStats;
    class User;

    class Track final : public Object<Track, TrackId>
    {
    public:
        struct FindParameters
        {
            std::vector<ClusterId> clusters;        // if non empty, tracks that belong to these clusters
            std::vector<std::string_view> keywords; // if non empty, name must match all of these keywords
            std::string name;                       // if non empty, must match this name
            std::string stem;                       // if non empty, must match this stem
            TrackSortMethod sortMethod{ TrackSortMethod::None };
            std::optional<Range> range;
            Wt::WDateTime writtenAfter;
            UserId starringUser;                                     // only tracks starred by this user
            std::optional<FeedbackBackend> feedbackBackend;          // and for this feedback backend
            ArtistId artist;                                         // only tracks that involve this artist
            std::string artistName;                                  // only tracks that involve this artist name
            core::EnumSet<TrackArtistLinkType> trackArtistLinkTypes; //    and for these link types
            bool nonRelease{};                                       // only tracks that do not belong to a release
            ReleaseId release;                                       // matching this release
            std::string releaseName;                                 // matching this release name
            TrackListId trackList;                                   // matching this trackList
            std::optional<int> trackNumber;                          // matching this track number
            std::optional<int> discNumber;                           // matching this disc number
            MediaLibraryId mediaLibrary;                             // If set, tracks in this library
            DirectoryId directory;                                   // if set, tracks in this directory
            std::optional<bool> hasEmbeddedImage;                    // if set, tracks that have or not embedded images

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
            FindParameters& setName(std::string_view _name)
            {
                name = _name;
                return *this;
            }
            FindParameters& setStem(std::string_view _stem)
            {
                stem = _stem;
                return *this;
            }
            FindParameters& setSortMethod(TrackSortMethod _method)
            {
                sortMethod = _method;
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
            FindParameters& setArtist(ArtistId _artist, core::EnumSet<TrackArtistLinkType> _trackArtistLinkTypes = {})
            {
                artist = _artist;
                trackArtistLinkTypes = _trackArtistLinkTypes;
                return *this;
            }
            FindParameters& setArtistName(std::string_view _artistName, core::EnumSet<TrackArtistLinkType> _trackArtistLinkTypes = {})
            {
                artistName = _artistName;
                trackArtistLinkTypes = _trackArtistLinkTypes;
                return *this;
            }
            FindParameters& setNonRelease(bool _nonRelease)
            {
                nonRelease = _nonRelease;
                return *this;
            }
            FindParameters& setRelease(ReleaseId _release)
            {
                release = _release;
                return *this;
            }
            FindParameters& setReleaseName(std::string_view _releaseName)
            {
                releaseName = _releaseName;
                return *this;
            }
            FindParameters& setTrackList(TrackListId _trackList)
            {
                trackList = _trackList;
                return *this;
            }
            FindParameters& setTrackNumber(int _trackNumber)
            {
                trackNumber = _trackNumber;
                return *this;
            }
            FindParameters& setDiscNumber(int _discNumber)
            {
                discNumber = _discNumber;
                return *this;
            }
            FindParameters& setMediaLibrary(MediaLibraryId _mediaLibrary)
            {
                mediaLibrary = _mediaLibrary;
                return *this;
            }
            FindParameters& setDirectory(DirectoryId _directory)
            {
                directory = _directory;
                return *this;
            }
            FindParameters& setHasEmbeddedImage(std::optional<bool> _hasEmbeddedImage)
            {
                hasEmbeddedImage = _hasEmbeddedImage;
                return *this;
            }
        };

        struct PathResult
        {
            TrackId trackId;
            std::filesystem::path path;
        };

        Track() = default;

        // Find utility functions
        static std::size_t getCount(Session& session);
        static pointer findByPath(Session& session, const std::filesystem::path& p);
        static pointer find(Session& session, TrackId id);
        static void find(Session& session, TrackId& lastRetrievedTrack, std::size_t count, const std::function<void(const Track::pointer&)>& func, MediaLibraryId library = {});
        static bool exists(Session& session, TrackId id);
        static std::vector<pointer> findByRecordingMBID(Session& session, const core::UUID& MBID);
        static std::vector<pointer> findByMBID(Session& session, const core::UUID& MBID);
        static RangeResults<TrackId> findSimilarTrackIds(Session& session, const std::vector<TrackId>& trackIds, std::optional<Range> range = std::nullopt);

        static RangeResults<TrackId> findIds(Session& session, const FindParameters& parameters);
        static RangeResults<pointer> find(Session& session, const FindParameters& parameters);
        static void find(Session& session, const FindParameters& parameters, const std::function<void(const Track::pointer&)>& func);
        static void find(Session& session, const FindParameters& parameters, bool& moreResults, const std::function<void(const Track::pointer&)>& func);
        static RangeResults<TrackId> findIdsTrackMBIDDuplicates(Session& session, std::optional<Range> range = std::nullopt);
        static RangeResults<TrackId> findIdsWithRecordingMBIDAndMissingFeatures(Session& session, std::optional<Range> range = std::nullopt);

        // Accessors
        void setScanVersion(std::size_t version) { _scanVersion = version; }
        void setTrackNumber(std::optional<int> num) { _trackNumber = num; }
        void setDiscNumber(std::optional<int> num) { _discNumber = num; }
        void setTotalTrack(std::optional<int> totalTrack) { _totalTrack = totalTrack; }
        void setDiscSubtitle(std::string_view name) { _discSubtitle = name; }
        void setName(std::string_view name);
        void setAbsoluteFilePath(const std::filesystem::path& filePath);
        void setRelativeFilePath(const std::filesystem::path& filePath);
        void setFileSize(std::size_t fileSize) { _fileSize = fileSize; }
        void setLastWriteTime(Wt::WDateTime time) { _fileLastWrite = time; }
        void setAddedTime(Wt::WDateTime time) { _fileAdded = time; }
        void setBitrate(std::size_t bitrate) { _bitrate = bitrate; }
        void setBitsPerSample(std::size_t bitsPerSample) { _bitsPerSample = bitsPerSample; }
        void setDuration(std::chrono::milliseconds duration) { _duration = duration; }
        void setChannelCount(std::size_t channelCount) { _channelCount = channelCount; }
        void setSampleRate(std::size_t channelCount) { _sampleRate = channelCount; }
        void setDate(const Wt::WDate& date) { _date = date; }
        void setYear(std::optional<int> year) { _year = year; }
        void setOriginalDate(const Wt::WDate& date) { _originalDate = date; }
        void setOriginalYear(std::optional<int> year) { _originalYear = year; }
        void setHasCover(bool hasCover) { _hasCover = hasCover; }
        void setTrackMBID(const std::optional<core::UUID>& MBID) { _trackMBID = MBID ? MBID->getAsString() : ""; }
        void setRecordingMBID(const std::optional<core::UUID>& MBID) { _recordingMBID = MBID ? MBID->getAsString() : ""; }
        void setCopyright(std::string_view copyright);
        void setCopyrightURL(std::string_view copyrightURL);
        void setTrackReplayGain(std::optional<float> replayGain) { _trackReplayGain = replayGain; }
        void setReleaseReplayGain(std::optional<float> replayGain) { _releaseReplayGain = replayGain; } // may be by disc!
        void setArtistDisplayName(std::string_view name) { _artistDisplayName = name; }
        void setComment(std::string_view comment) { _comment = comment; }
        void clearArtistLinks();
        void addArtistLink(const ObjectPtr<TrackArtistLink>& artistLink);
        void setRelease(ObjectPtr<Release> release) { _release = getDboPtr(release); }
        void setClusters(const std::vector<ObjectPtr<Cluster>>& clusters);
        void clearLyrics();
        void clearEmbeddedLyrics();
        void addLyrics(const ObjectPtr<TrackLyrics>& lyrics);
        void setMediaLibrary(ObjectPtr<MediaLibrary> mediaLibrary) { _mediaLibrary = getDboPtr(mediaLibrary); }
        void setDirectory(ObjectPtr<Directory> directory) { _directory = getDboPtr(directory); }

        std::size_t getScanVersion() const { return _scanVersion; }
        std::optional<std::size_t> getTrackNumber() const { return _trackNumber; }
        std::optional<std::size_t> getTotalTrack() const { return _totalTrack; }
        std::optional<std::size_t> getDiscNumber() const { return _discNumber; }
        const std::string& getDiscSubtitle() const { return _discSubtitle; }
        std::string getName() const { return _name; }
        const std::filesystem::path& getAbsoluteFilePath() const { return _absoluteFilePath; }
        const std::filesystem::path& getRelativeFilePath() const { return _relativeFilePath; }
        long long getFileSize() const { return _fileSize; }
        std::size_t getBitrate() const { return _bitrate; }
        std::size_t getBitsPerSample() const { return _bitsPerSample; }
        std::size_t getChannelCount() const { return _channelCount; }
        std::chrono::milliseconds getDuration() const { return _duration; }
        std::size_t getSampleRate() const { return _sampleRate; }
        const Wt::WDateTime& getLastWritten() const { return _fileLastWrite; }
        const Wt::WDate& getDate() const { return _date; }
        std::optional<int> getYear() const { return _year; }
        const Wt::WDate& getOriginalDate() const { return _originalDate; }
        std::optional<int> getOriginalYear() const { return _originalYear; };
        const Wt::WDateTime& getLastWriteTime() const { return _fileLastWrite; }
        const Wt::WDateTime& getAddedTime() const { return _fileAdded; }
        bool hasCover() const { return _hasCover; }
        bool hasLyrics() const;
        std::optional<core::UUID> getTrackMBID() const { return core::UUID::fromString(_trackMBID); }
        std::optional<core::UUID> getRecordingMBID() const { return core::UUID::fromString(_recordingMBID); }
        std::optional<std::string> getCopyright() const;
        std::optional<std::string> getCopyrightURL() const;
        std::optional<float> getTrackReplayGain() const { return _trackReplayGain; }
        std::optional<float> getReleaseReplayGain() const { return _releaseReplayGain; }
        std::string_view getArtistDisplayName() const { return _artistDisplayName; }
        std::string_view getComment() const { return _comment; }

        // no artistLinkTypes means get all
        std::vector<ObjectPtr<Artist>> getArtists(core::EnumSet<TrackArtistLinkType> artistLinkTypes) const; // no type means all
        std::vector<ArtistId> getArtistIds(core::EnumSet<TrackArtistLinkType> artistLinkTypes) const;        // no type means all
        std::vector<ObjectPtr<TrackArtistLink>> getArtistLinks() const;
        ObjectPtr<Release> getRelease() const { return _release; }
        std::vector<ObjectPtr<Cluster>> getClusters() const;
        std::vector<ClusterId> getClusterIds() const;
        ObjectPtr<MediaLibrary> getMediaLibrary() const { return _mediaLibrary; }
        ObjectPtr<Directory> getDirectory() const { return _directory; }

        std::vector<std::vector<ObjectPtr<Cluster>>> getClusterGroups(const std::vector<ClusterTypeId>& clusterTypes, std::size_t size) const;

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _scanVersion, "scan_version");
            Wt::Dbo::field(a, _trackNumber, "track_number");
            Wt::Dbo::field(a, _discNumber, "disc_number");
            Wt::Dbo::field(a, _totalTrack, "total_track");     // here in Track since Release does not have concept of "disc" (yet?)
            Wt::Dbo::field(a, _discSubtitle, "disc_subtitle"); // here in Track since Release does not have concept of "disc" (yet?)
            Wt::Dbo::field(a, _name, "name");
            Wt::Dbo::field(a, _duration, "duration");
            Wt::Dbo::field(a, _bitrate, "bitrate");
            Wt::Dbo::field(a, _bitsPerSample, "bits_per_sample");
            Wt::Dbo::field(a, _channelCount, "channel_count");
            Wt::Dbo::field(a, _sampleRate, "sample_rate");
            Wt::Dbo::field(a, _date, "date");
            Wt::Dbo::field(a, _year, "year");
            Wt::Dbo::field(a, _originalDate, "original_date");
            Wt::Dbo::field(a, _originalYear, "original_year");
            Wt::Dbo::field(a, _absoluteFilePath, "absolute_file_path");
            Wt::Dbo::field(a, _relativeFilePath, "relative_file_path");
            Wt::Dbo::field(a, _fileStem, "file_stem");
            Wt::Dbo::field(a, _fileSize, "file_size");
            Wt::Dbo::field(a, _fileLastWrite, "file_last_write");
            Wt::Dbo::field(a, _fileAdded, "file_added");
            Wt::Dbo::field(a, _hasCover, "has_cover");
            Wt::Dbo::field(a, _trackMBID, "mbid");
            Wt::Dbo::field(a, _recordingMBID, "recording_mbid");
            Wt::Dbo::field(a, _copyright, "copyright");
            Wt::Dbo::field(a, _copyrightURL, "copyright_url");
            Wt::Dbo::field(a, _trackReplayGain, "track_replay_gain");
            Wt::Dbo::field(a, _releaseReplayGain, "release_replay_gain"); // here in Track since Release does not have concept of "disc" (yet?)
            Wt::Dbo::field(a, _artistDisplayName, "artist_display_name");
            Wt::Dbo::field(a, _comment, "comment");

            Wt::Dbo::belongsTo(a, _release, "release", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::belongsTo(a, _mediaLibrary, "media_library", Wt::Dbo::OnDeleteSetNull); // don't delete track on media library removal, we want to wait for the next scan to have a chance to migrate files
            Wt::Dbo::belongsTo(a, _directory, "directory", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::hasMany(a, _trackArtistLinks, Wt::Dbo::ManyToOne, "track");
            Wt::Dbo::hasMany(a, _clusters, Wt::Dbo::ManyToMany, "track_cluster", "", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::hasMany(a, _trackLyrics, Wt::Dbo::ManyToOne, "track");
        }

    private:
        friend class Session;
        static pointer create(Session& session);

        static constexpr std::size_t _maxNameLength{ 512 };
        static constexpr std::size_t _maxCopyrightLength{ 512 };
        static constexpr std::size_t _maxCopyrightURLLength{ 512 };

        int _scanVersion{};
        std::optional<int> _trackNumber{};
        std::optional<int> _discNumber{};
        std::optional<int> _totalTrack{};
        std::string _discSubtitle;
        std::string _name;
        int _bitrate{}; // in bps
        int _bitsPerSample{};
        int _channelCount{};
        std::chrono::duration<int, std::milli> _duration{};
        int _sampleRate{};
        Wt::WDate _date;
        std::optional<int> _year;
        Wt::WDate _originalDate;
        std::optional<int> _originalYear;
        std::filesystem::path _absoluteFilePath; // full path
        std::filesystem::path _relativeFilePath; // relative to root (that may be deleted)
        std::filesystem::path _fileStem;
        long long _fileSize{};
        Wt::WDateTime _fileLastWrite;
        Wt::WDateTime _fileAdded;
        bool _hasCover{};
        std::string _trackMBID;
        std::string _recordingMBID;
        std::string _copyright;
        std::string _copyrightURL;
        std::optional<float> _trackReplayGain;
        std::optional<float> _releaseReplayGain;
        std::string _artistDisplayName;
        std::string _comment;

        Wt::Dbo::ptr<Release> _release;
        Wt::Dbo::ptr<MediaLibrary> _mediaLibrary;
        Wt::Dbo::ptr<Directory> _directory;
        Wt::Dbo::collection<Wt::Dbo::ptr<TrackArtistLink>> _trackArtistLinks;
        Wt::Dbo::collection<Wt::Dbo::ptr<Cluster>> _clusters;
        Wt::Dbo::collection<Wt::Dbo::ptr<TrackLyrics>> _trackLyrics;
    };

    namespace Debug
    {
        struct TrackInfo
        {
            Session& session;
            TrackId trackId;
        };
        std::ostream& operator<<(std::ostream& os, const TrackInfo& trackInfo);
    } // namespace Debug
} // namespace lms::db
