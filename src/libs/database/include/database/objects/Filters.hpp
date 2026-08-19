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

#include <optional>
#include <span>
#include <vector>

#include "core/media/Codec.hpp"

#include "database/objects/ClusterId.hpp"
#include "database/objects/GenreId.hpp"
#include "database/objects/GroupingId.hpp"
#include "database/objects/LabelId.hpp"
#include "database/objects/LanguageId.hpp"
#include "database/objects/MediaLibraryId.hpp"
#include "database/objects/MoodId.hpp"
#include "database/objects/ReleaseTypeId.hpp"

namespace lms::db
{
    struct Filters
    {
        MediaLibraryId mediaLibrary;             // tracks that belongs to this library
        std::vector<ClusterId> clusters;         // tracks that belong to *all* these clusters
        GenreId genre;                           // tracks that belong to this genre
        GroupingId grouping;                     // tracks that belong to this grouping
        LabelId label;                           // tracks which release has this label
        LanguageId language;                     // tracks that belong to this language
        MoodId mood;                             // tracks that belong to this mood
        ReleaseTypeId releaseType;               // tracks which release has this type
        std::optional<core::media::Codec> codec; // tracks that match this codec

        Filters& setClusters(std::span<const ClusterId> _clusters)
        {
            clusters.assign(std::cbegin(_clusters), std::cend(_clusters));
            return *this;
        }
        Filters& setGenre(GenreId _genre)
        {
            genre = _genre;
            return *this;
        }
        Filters& setGrouping(GroupingId _grouping)
        {
            grouping = _grouping;
            return *this;
        }
        Filters& setLanguage(LanguageId _language)
        {
            language = _language;
            return *this;
        }
        Filters& setMood(MoodId _mood)
        {
            mood = _mood;
            return *this;
        }
        Filters& setMediaLibrary(MediaLibraryId _mediaLibrary)
        {
            mediaLibrary = _mediaLibrary;
            return *this;
        }
        Filters& setLabel(LabelId _label)
        {
            label = _label;
            return *this;
        }
        Filters& setReleaseType(ReleaseTypeId _releaseType)
        {
            releaseType = _releaseType;
            return *this;
        }
        Filters& setCodec(std::optional<core::media::Codec> _codec)
        {
            codec = _codec;
            return *this;
        }
    };
} // namespace lms::db