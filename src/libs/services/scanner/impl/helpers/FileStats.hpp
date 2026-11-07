/*
 * Copyright (C) 2026 Emeric Poupon
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

#include <cstddef>

namespace lms::db
{
    class Session;
}

namespace lms::scanner::helpers
{
    struct FileStats
    {
        std::size_t trackCount;
        std::size_t imageCount;
        std::size_t trackLyricsCount;
        std::size_t playListCount;
        std::size_t artistInfoCount;

        std::size_t getTotalFileCount() const { return trackCount + imageCount + trackLyricsCount + playListCount + artistInfoCount; }
    };

    // The caller must hold a read transaction
    FileStats getFileStats(db::Session& session);
} // namespace lms::scanner::helpers
