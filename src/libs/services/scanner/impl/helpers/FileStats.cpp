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

#include "FileStats.hpp"

#include "database/ISession.hpp"
#include "database/objects/ArtistInfo.hpp"
#include "database/objects/Image.hpp"
#include "database/objects/PlayListFile.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackLyrics.hpp"

namespace lms::scanner::helpers
{
    FileStats getFileStats(db::ISession& session)
    {
        FileStats stats{};

        stats.trackCount = db::Track::getCount(session);
        stats.artistInfoCount = db::ArtistInfo::getCount(session);
        stats.imageCount = db::Image::getCount(session);
        stats.trackLyricsCount = db::TrackLyrics::getExternalLyricsCount(session);
        stats.playListCount = db::PlayListFile::getCount(session);

        return stats;
    }
} // namespace lms::scanner::helpers
