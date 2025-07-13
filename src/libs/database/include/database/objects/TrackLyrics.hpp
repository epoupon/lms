/*
 * Copyright (C) 2024 Emeric Poupon
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
#include <map>
#include <optional>
#include <span>
#include <string>

#include <Wt/Dbo/Field.h>

#include "database/IdType.hpp"
#include "database/Object.hpp"
#include "database/Types.hpp"
#include "database/objects/TrackId.hpp"

LMS_DECLARE_IDTYPE(TrackLyricsId)

namespace lms::db
{
    class Directory;
    class Session;
    class Track;

    class TrackLyrics final : public Object<TrackLyrics, TrackLyricsId>
    {
    public:
        TrackLyrics() = default;

        struct FindParameters
        {
            std::optional<Range> range;
            TrackId track;
            std::optional<bool> external; // if set, true means external, false means embedded
            TrackLyricsSortMethod sortMethod{ TrackLyricsSortMethod::None };

            FindParameters& setRange(std::optional<Range> _range)
            {
                range = _range;
                return *this;
            }
            FindParameters& setTrack(TrackId _track)
            {
                track = _track;
                return *this;
            }
            FindParameters& setExternal(std::optional<bool> _external)
            {
                external = _external;
                return *this;
            }
            FindParameters& setSortMethod(TrackLyricsSortMethod _sortMethod)
            {
                sortMethod = _sortMethod;
                return *this;
            }
        };

        // Find utilities
        static std::size_t getCount(Session& session);
        static std::size_t getExternalLyricsCount(Session& session);
        static pointer find(Session& session, TrackLyricsId id);
        static pointer find(Session& session, const std::filesystem::path& file);
        static void find(Session& session, const FindParameters& params, const std::function<void(const TrackLyrics::pointer&)>& func);
        static void find(Session& session, TrackLyricsId& lastRetrievedId, std::size_t count, const std::function<void(const TrackLyrics::pointer&)>& func);
        static RangeResults<TrackLyricsId> findOrphanIds(Session& session, std::optional<Range> range);
        static void findAbsoluteFilePath(Session& session, TrackLyricsId& lastRetrievedId, std::size_t count, const std::function<void(TrackLyricsId trackLyricsId, const std::filesystem::path& absoluteFilePath)>& func);

        using SynchronizedLines = std::map<std::chrono::milliseconds, std::string>;

        // Readers
        const std::filesystem::path& getAbsoluteFilePath() const { return _fileAbsolutePath; }
        std::string_view getFileStem() const { return _fileStem; }
        const Wt::WDateTime& getLastWriteTime() const { return _fileLastWrite; }
        std::size_t getFileSize() const { return _fileSize; }
        std::string_view getLanguage() const { return _language; }
        std::string_view getDisplayArtist() const { return _displayArtist; }
        std::string_view getDisplayTitle() const { return _displayTitle; }
        std::chrono::milliseconds getOffset() const { return _offset; }
        bool isSynchronized() const { return _synchronized; }
        SynchronizedLines getSynchronizedLines() const;
        std::vector<std::string> getUnsynchronizedLines() const;
        Wt::Dbo::ptr<Track> getTrack() const { return _track; }
        Wt::Dbo::ptr<Directory> getDirectory() const { return _directory; }

        // Writers
        void setAbsoluteFilePath(const std::filesystem::path& path);
        void setLastWriteTime(const Wt::WDateTime& fileLastWrite) { _fileLastWrite = fileLastWrite; }
        void setFileSize(std::size_t fileSize) { _fileSize = fileSize; }
        void setLanguage(std::string_view language) { _language = language; }
        void setOffset(std::chrono::milliseconds offset) { _offset = offset; }
        void setDisplayArtist(std::string_view displayArtist) { _displayArtist = displayArtist; }
        void setDisplayTitle(std::string_view displayTitle) { _displayTitle = displayTitle; }
        void setSynchronizedLines(const SynchronizedLines& lines);
        void setUnsynchronizedLines(std::span<const std::string> lines);
        void setTrack(const ObjectPtr<Track>& track) { _track = getDboPtr(track); }
        void setDirectory(const ObjectPtr<Directory>& directory) { _directory = getDboPtr(directory); }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _fileAbsolutePath, "absolute_file_path");
            Wt::Dbo::field(a, _fileStem, "stem");
            Wt::Dbo::field(a, _fileLastWrite, "file_last_write");
            Wt::Dbo::field(a, _fileSize, "file_size");
            Wt::Dbo::field(a, _lines, "lines");
            Wt::Dbo::field(a, _language, "language");
            Wt::Dbo::field(a, _offset, "offset");
            Wt::Dbo::field(a, _displayArtist, "display_artist");
            Wt::Dbo::field(a, _displayTitle, "display_title");
            Wt::Dbo::field(a, _synchronized, "synchronized");
            Wt::Dbo::belongsTo(a, _track, "track", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::belongsTo(a, _directory, "directory", Wt::Dbo::OnDeleteCascade);
        }

    private:
        friend class Session;
        static pointer create(Session& session);

        std::filesystem::path _fileAbsolutePath; // optional (if embedded in media file)
        std::string _fileStem;                   // optional (if embedded in media file)
        Wt::WDateTime _fileLastWrite;            // optional (if embedded in media file)
        int _fileSize{};                         // optional (if embedded in media file)
        std::string _lines;                      // a json encoded array of lines (with possibly offsets)
        std::string _language;
        std::chrono::duration<int, std::milli> _offset{};
        std::string _displayArtist;
        std::string _displayTitle;
        bool _synchronized{};

        Wt::Dbo::ptr<Track> _track;
        Wt::Dbo::ptr<Directory> _directory;
    };
} // namespace lms::db
