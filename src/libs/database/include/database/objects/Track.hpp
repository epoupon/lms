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
#include <functional>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <Wt/WDateTime.h>

#include "core/EnumSet.hpp"
#include "core/PartialDateTime.hpp"
#include "core/UUID.hpp"
#include "core/media/Codec.hpp"
#include "core/media/Container.hpp"

#include "database/IdRange.hpp"
#include "database/Types.hpp"
#include "database/objects/ArtistId.hpp"
#include "database/objects/ArtworkId.hpp"
#include "database/objects/ClusterId.hpp"
#include "database/objects/DirectoryId.hpp"
#include "database/objects/FilterExpr.hpp"
#include "database/objects/GenreId.hpp"
#include "database/objects/GroupingId.hpp"
#include "database/objects/LanguageId.hpp"
#include "database/objects/MediaLibraryId.hpp"
#include "database/objects/MediumId.hpp"
#include "database/objects/MoodId.hpp"
#include "database/objects/MovementId.hpp"
#include "database/objects/PlayQueueId.hpp"
#include "database/objects/ReleaseId.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/TrackListId.hpp"
#include "database/objects/Types.hpp"
#include "database/objects/UserId.hpp"
#include "database/objects/WorkId.hpp"
#include "database/objects/detail/Object.hpp"
#include "database/objects/detail/Types.hpp"

namespace lms::db
{
    class ISession;

    class Track : public detail::Object<Track, TrackId>
    {
    public:
        struct FindParameters
        {
            std::optional<FilterExpr> filter;       // if set, tracks must match this expression
            UserId filterUser;                      // required if filter has per-user fields
            std::vector<std::string_view> keywords; // if non empty, name must match all of these keywords
            std::string name;                       // if non empty, must match this name (title)
            TrackSortMethod sortMethod{ TrackSortMethod::None };
            std::optional<Range> range;
            UserId feedbackUser;                                     // if valid, only tracks that have feedback from this user
            std::optional<FeedbackValue> feedbackValue;              // if set, only tracks with feedback matching this value (any user, unless feedbackUser is also set)
            ArtistId artist;                                         // only tracks that involve this artist
            std::string artistName;                                  // only tracks that involve this artist name
            core::EnumSet<TrackArtistLinkType> trackArtistLinkTypes; //    and for these link types
            bool nonRelease{};                                       // only tracks that do not belong to a release
            MediumId medium;                                         // matching this medium
            TrackListId trackList;                                   // in this track list, in list order unless sorted (once per entry)
            PlayQueueId playQueue;                                   // in this play queue
            ReleaseId release;                                       // matching this release
            std::string releaseName;                                 // matching this release name
            std::optional<int> trackNumber;                          // matching this track number
            DirectoryId directory;                                   // if set, tracks in this directory
            std::optional<std::size_t> fileSize;                     // if set, tracks that match this file size
            std::optional<bool> hasMusicNNEmbeddings;                // If set, tracks that have (or not) MusicNN embeddings
            TrackId lastTrackId;                                     // If set, tracks that are after this one, must be used with sort by id

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
            FindParameters& setTrackList(TrackListId _trackList)
            {
                trackList = _trackList;
                return *this;
            }
            FindParameters& setPlayQueue(PlayQueueId _playQueue)
            {
                playQueue = _playQueue;
                return *this;
            }
            FindParameters& setMedium(MediumId _medium)
            {
                medium = _medium;
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
            FindParameters& setTrackNumber(int _trackNumber)
            {
                trackNumber = _trackNumber;
                return *this;
            }
            FindParameters& setDirectory(DirectoryId _directory)
            {
                directory = _directory;
                return *this;
            }
            FindParameters& setFileSize(std::optional<std::size_t> _fileSize)
            {
                fileSize = _fileSize;
                return *this;
            }
            FindParameters& setHasMusicNNEmbeddings(std::optional<bool> _hasMusicNNEmbeddings)
            {
                hasMusicNNEmbeddings = _hasMusicNNEmbeddings;
                return *this;
            }
            FindParameters& setLastTrackId(TrackId _lastTrackId)
            {
                lastTrackId = _lastTrackId;
                return *this;
            }
        };

        static Track create(ISession& session);
        static void destroy(ISession& session, std::span<const TrackId> ids);

        // Find utility functions
        static std::size_t getCount(ISession& session);
        static std::optional<Track> findByPath(ISession& session, const std::filesystem::path& p);
        static std::optional<FileInfo> findFileInfo(ISession& session, const std::filesystem::path& p);
        static std::optional<Track> find(ISession& session, TrackId id);
        static void find(ISession& session, TrackId& lastRetrievedId, std::size_t count, const std::function<void(const Track&)>& func, MediaLibraryId library = {});
        static void find(ISession& session, const IdRange<TrackId>& idRange, const std::function<void(const Track&)>& func);
        static IdRange<TrackId> findNextIdRange(ISession& session, TrackId lastRetrievedId, std::size_t count);

        using TrackLocationVisitor = std::function<void(TrackId trackId, const std::filesystem::path& absoluteFilePath)>;
        static void findAbsoluteFilePath(ISession& session, TrackId& lastRetrievedId, std::size_t count, const TrackLocationVisitor& func);
        static void findAbsoluteFilePath(ISession& session, const FindParameters& params, const TrackLocationVisitor& func);

        static bool exists(ISession& session, TrackId id);
        static std::vector<Track> findByRecordingMBID(ISession& session, const core::UUID& MBID);
        static std::vector<Track> findByMBID(ISession& session, const core::UUID& MBID);
        static std::vector<TrackId> findIds(ISession& session, const FindParameters& params);
        static std::vector<Track> find(ISession& session, const FindParameters& params);
        static void find(ISession& session, const FindParameters& params, const std::function<void(const Track&)>& func);
        static std::size_t getCount(ISession& session, const FindParameters& params);
        static std::vector<TrackId> findIdsTrackMBIDDuplicates(ISession& session, std::optional<Range> range = std::nullopt);
        static bool hasLyrics(ISession& session, TrackId id);
        static bool hasWork(ISession& session, TrackId id);
        static bool hasMovement(ISession& session, TrackId id);

        // Update utility functions
        static void update(ISession& session, const Track& track);
        static void updatePreferredArtwork(ISession& session, TrackId trackId, ArtworkId artworkId);
        static void updatePreferredMediaArtwork(ISession& session, TrackId trackId, ArtworkId artworkId);
        static void clearArtistLinks(ISession& session, TrackId id);
        static void setClusters(ISession& session, TrackId id, std::span<const ClusterId> clusters);
        static void setGenres(ISession& session, TrackId id, std::span<const GenreId> genres);
        static void setGroupings(ISession& session, TrackId id, std::span<const GroupingId> groupings);
        static void setLanguages(ISession& session, TrackId id, std::span<const LanguageId> languages);
        static void setMoods(ISession& session, TrackId id, std::span<const MoodId> moods);
        static void setWorks(ISession& session, TrackId id, std::span<const WorkId> works);
        static void clearMovements(ISession& session, TrackId id);
        static void clearEmbeddedLyrics(ISession& session, TrackId id);
        static void clearEmbeddedImageLinks(ISession& session, TrackId id);

        // Setters
        void setScanVersion(std::size_t version) { _scanVersion = version; }

        // File properties
        void setAbsoluteFilePath(const std::filesystem::path& filePath);
        void setFileSize(std::size_t fileSize) { _fileSize = fileSize; }
        void setLastWriteTime(const Wt::WDateTime& time) { _fileLastWrite = time; }
        void setAddedTime(const Wt::WDateTime& time) { _fileAdded = time; }

        // Audio properties
        void setDuration(std::chrono::milliseconds duration) { _duration = duration; }
        void setContainer(core::media::Container container);
        void setCodec(core::media::Codec codec);
        void setBitrate(std::size_t bitrate) { _bitrate = bitrate; }
        void setChannelCount(std::size_t channelCount) { _channelCount = channelCount; }
        void setSampleRate(std::size_t sampleRate) { _sampleRate = sampleRate; }
        void setBitsPerSample(std::optional<std::size_t> bitsPerSample) { _bitsPerSample = bitsPerSample; }
        void setReplayGain(std::optional<float> replayGain) { _replayGain = replayGain; }

        // Metadata
        void setTrackNumber(std::optional<int> num) { _trackNumber = num; }
        void setName(std::string_view name);
        void setDate(const core::PartialDateTime& date) { _date = date; }
        void setOriginalDate(const core::PartialDateTime& date) { _originalDate = date; }
        void setTrackMBID(const std::optional<core::UUID>& MBID) { _trackMBID = MBID; }
        void setRecordingMBID(const std::optional<core::UUID>& MBID) { _recordingMBID = MBID; }
        void setCopyright(std::string_view copyright);
        void setCopyrightURL(std::string_view copyrightURL);
        void setAdvisory(Advisory advisory) { _advisory = advisory; }
        void setArtistDisplayName(std::string_view name) { _artistDisplayName = name; }
        void setComment(std::string_view comment) { _comment = comment; }
        void setRelease(ReleaseId release) { _releaseId = release; }
        void setMedium(MediumId medium) { _mediumId = medium; }
        void setMediaLibrary(MediaLibraryId mediaLibrary) { _mediaLibraryId = mediaLibrary; }
        void setDirectory(DirectoryId directory) { _directoryId = directory; }
        void setPreferredArtwork(ArtworkId artwork) { _preferredArtworkId = artwork; }
        void setPreferredMediaArtwork(ArtworkId artwork) { _preferredMediaArtworkId = artwork; }

        // Getters
        std::size_t getScanVersion() const { return _scanVersion; }

        // File properties
        const std::filesystem::path& getAbsoluteFilePath() const { return _absoluteFilePath; }
        long long getFileSize() const { return _fileSize; }
        const Wt::WDateTime& getLastWritten() const { return _fileLastWrite; }
        const Wt::WDateTime& getAddedTime() const { return _fileAdded; }

        // Audio properties
        std::chrono::milliseconds getDuration() const { return _duration; }
        std::optional<core::media::Container> getContainer() const;
        std::optional<core::media::Codec> getCodec() const;
        std::size_t getBitrate() const { return _bitrate; }
        std::size_t getChannelCount() const { return _channelCount; }
        std::size_t getSampleRate() const { return _sampleRate; }
        std::optional<std::size_t> getBitsPerSample() const { return _bitsPerSample; }
        std::optional<float> getReplayGain() const { return _replayGain; }

        // Metadata
        std::optional<std::size_t> getTrackNumber() const { return _trackNumber; }
        std::string_view getName() const { return _name; }
        const core::PartialDateTime& getDate() const { return _date; }
        std::optional<int> getYear() const;
        const core::PartialDateTime& getOriginalDate() const { return _originalDate; }
        std::optional<int> getOriginalYear() const;
        const Wt::WDateTime& getLastWriteTime() const { return _fileLastWrite; }
        std::optional<core::UUID> getTrackMBID() const { return _trackMBID; }
        std::optional<core::UUID> getRecordingMBID() const { return _recordingMBID; }
        std::string_view getCopyright() const { return _copyright; }
        std::string_view getCopyrightURL() const { return _copyrightURL; }
        std::string_view getArtistDisplayName() const { return _artistDisplayName; }
        std::string_view getComment() const { return _comment; }
        Advisory getAdvisory() const { return _advisory; }

        // Relations
        ReleaseId getReleaseId() const { return _releaseId; }
        MediumId getMediumId() const { return _mediumId; }
        MediaLibraryId getMediaLibraryId() const { return _mediaLibraryId; }
        DirectoryId getDirectoryId() const { return _directoryId; }
        ArtworkId getPreferredArtworkId() const { return _preferredArtworkId; }
        ArtworkId getPreferredMediaArtworkId() const { return _preferredMediaArtworkId; }

    protected:
        Track() = default;
        static constexpr std::size_t _maxNameLength{ 512 };
        static constexpr std::size_t _maxCopyrightLength{ 512 };
        static constexpr std::size_t _maxCopyrightURLLength{ 512 };

        int _scanVersion{};

        // File information
        std::filesystem::path _absoluteFilePath; // full path
        long long _fileSize{};
        Wt::WDateTime _fileLastWrite;
        Wt::WDateTime _fileAdded;

        // Audio properties
        std::chrono::duration<int, std::milli> _duration{};
        detail::Container _container{ detail::Container::Unknown };
        detail::Codec _codec{ detail::Codec::Unknown };
        int _bitrate{}; // in bps
        int _channelCount{};
        int _sampleRate{};
        std::optional<int> _bitsPerSample;
        std::optional<float> _replayGain;

        // Metadata
        std::optional<int> _trackNumber;
        std::string _name;
        core::PartialDateTime _date;
        core::PartialDateTime _originalDate;
        std::optional<core::UUID> _trackMBID;
        std::optional<core::UUID> _recordingMBID;
        std::string _copyright;
        std::string _copyrightURL;
        std::string _artistDisplayName;
        std::string _comment;
        Advisory _advisory{ Advisory::UnSet };

        ReleaseId _releaseId;
        MediumId _mediumId;
        MediaLibraryId _mediaLibraryId;
        DirectoryId _directoryId;
        ArtworkId _preferredArtworkId;
        ArtworkId _preferredMediaArtworkId;
    };

    namespace Debug
    {
        struct TrackInfo
        {
            ISession& session;
            TrackId trackId;
        };
        std::ostream& operator<<(std::ostream& os, const TrackInfo& trackInfo);
    } // namespace Debug
} // namespace lms::db
