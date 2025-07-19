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

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>

#include <Wt/WDate.h>

#include "core/Exception.hpp"
#include "core/TaggedType.hpp"

namespace lms::db
{
    class Exception : public core::LmsException
    {
    public:
        using LmsException::LmsException;
    };

    // Caution: do not change enum values if they are set!

    // Request:
    // 	  size = 0 => means we don't want data
    // Response (via RangeResults)
    //    size => results size
    struct Range
    {
        std::size_t offset{};
        std::size_t size{};

        bool operator==(const Range& rhs) const { return offset == rhs.offset && size == rhs.size; }
    };

    // Func must return true to continue iterating
    template<typename Func>
    void foreachSubRange(Range range, std::size_t subRangeSize, Func&& func)
    {
        assert(subRangeSize > 0);

        Range subRange{ range.offset, std::min(range.size, subRangeSize) };
        while (subRange.size > 0)
        {
            if (!func(subRange))
                break;

            subRange.offset += subRange.size;
            subRange.size = std::min(subRangeSize, range.size - (subRange.offset - range.offset));
        }
    }

    template<typename T>
    struct RangeResults
    {
        Range range;
        std::vector<T> results;
        bool moreResults{};

        RangeResults getSubRange(Range subRange)
        {
            assert(subRange.offset >= range.offset);

            if (!subRange.size)
                subRange.size = range.size - (subRange.offset - range.offset);

            subRange.offset = std::min(subRange.offset, range.offset + range.size);
            subRange.size = std::min(subRange.size, range.offset + range.size - subRange.offset);

            RangeResults subResults;

            auto itBegin{ std::cbegin(results) + subRange.offset - range.offset };
            auto itEnd{ itBegin + subRange.size };
            subResults.results.reserve(std::distance(itBegin, itEnd));
            std::copy(itBegin, itEnd, std::back_inserter(subResults.results));

            subResults.range = subRange;
            if (subRange.offset + subRange.size == range.offset + range.size)
                subResults.moreResults = moreResults;
            else
                subResults.moreResults = true;

            return subResults;
        }
    };

    struct FileStats
    {
        std::size_t trackCount;
        std::size_t imageCount;
        std::size_t trackLyricsCount;
        std::size_t playListCount;
        std::size_t artistInfoCount;

        std::size_t getTotalFileCount() const { return trackCount + imageCount + trackLyricsCount + playListCount + artistInfoCount; }
    };

    struct YearRange
    {
        int begin{};
        int end{};
    };

    struct DiscInfo
    {
        std::size_t position;
        std::string name;
    };

    struct FileInfo
    {
        Wt::WDateTime lastWrittenTime;
        std::size_t scanVersion{};
    };

    enum class ArtistSortMethod
    {
        None,
        Id,
        Name,
        SortName,
        Random,
        LastWrittenDesc,
        AddedDesc,
        StarredDateDesc,
    };

    enum class ClusterSortMethod
    {
        None,
        Name,
    };

    enum class DirectorySortMethod
    {
        None,
        Name,
    };

    using ImageHashType = core::TaggedType<class ImageHash, std::uint64_t>;

    enum class LabelSortMethod
    {
        None,
        Name,
    };

    enum class ReleaseSortMethod
    {
        None,
        Id,
        Name,
        SortName,
        ArtistNameThenName,
        DateAsc,
        DateDesc,
        OriginalDate,
        OriginalDateDesc,
        Random,
        LastWrittenDesc,
        AddedDesc,
        StarredDateDesc,
    };

    enum class ReleaseTypeSortMethod
    {
        None,
        Name,
    };

    enum class TrackEmbeddedImageSortMethod
    {
        None,
        SizeDesc,
        TrackNumberThenSizeDesc,
        DiscNumberThenTrackNumberThenSizeDesc,
        TrackListIndexAscThenSizeDesc,
    };

    enum class TrackListSortMethod
    {
        None,
        Name,
        LastModifiedDesc,
    };

    enum class TrackSortMethod
    {
        None,
        Id,
        Random,
        LastWrittenDesc,
        AddedDesc,
        StarredDateDesc,
        AbsoluteFilePath,
        Name,
        DateDescAndRelease,
        Release,   // order by disc/track number
        TrackList, // order by asc order in tracklist
    };

    enum class TrackLyricsSortMethod
    {
        None,
        ExternalFirst,
        EmbeddedFirst,
    };

    enum class ImageType
    {
        Unknown = 0,
        Other = 1,
        FileIcon = 2,
        OtherFileIcon = 3,
        FrontCover = 4,
        BackCover = 5,
        LeafletPage = 6,
        Media = 7,
        LeadArtist = 8,
        Artist = 9,
        Conductor = 10,
        Band = 11,
        Composer = 12,
        Lyricist = 13,
        RecordingLocation = 14,
        DuringRecording = 15,
        DuringPerformance = 16,
        MovieScreenCapture = 17,
        ColouredFish = 18,
        Illustration = 19,
        BandLogo = 20,
        PublisherLogo = 21
    };

    enum class TrackArtistLinkType
    {
        Artist = 0, // regular track artist
        Arranger = 1,
        Composer = 2,
        Conductor = 3,
        Lyricist = 4,
        Mixer = 5,
        Performer = 6,
        Producer = 7,
        ReleaseArtist = 8,
        Remixer = 9,
        Writer = 10,
    };

    // User selectable transcoding output formats
    enum class TranscodingOutputFormat
    {
        MP3 = 1,
        OGG_OPUS = 2,
        OGG_VORBIS = 3,
        WEBM_VORBIS = 4,
        MATROSKA_OPUS = 5,
    };

    using Bitrate = std::uint32_t;
    // Do not remove values!
    void visitAllowedAudioBitrates(std::function<void(Bitrate)>);
    bool isAudioBitrateAllowed(Bitrate bitrate);

    using Rating = int;

    enum class ScrobblingBackend
    {
        Internal = 0,
        ListenBrainz = 1,
    };

    enum class FeedbackBackend
    {
        Internal = 0,
        ListenBrainz = 1,
    };

    enum class SyncState
    {
        PendingAdd = 0,
        Synchronized = 1,
        PendingRemove = 2,
    };

    enum class UserType
    {
        REGULAR = 0,
        ADMIN = 1,
        DEMO = 2,
    };

    enum class UITheme
    {
        Light = 0,
        Dark = 1,
    };

    enum class SubsonicArtistListMode
    {
        AllArtists = 0,
        ReleaseArtists = 1,
        TrackArtists = 2,
    };

    enum class TrackListType
    {
        PlayList = 0, // user controlled playlists
        Internal = 1, // internal usage (current playqueue, history, ...)
    };

    enum class Advisory
    {
        UnSet = 0,
        Unknown = 1,
        Clean = 2,
        Explicit = 3,
    };
} // namespace lms::db
