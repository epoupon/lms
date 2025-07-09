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

#include <span>
#include <vector>

#include "database/objects/ClusterId.hpp"
#include "database/objects/LabelId.hpp"
#include "database/objects/MediaLibraryId.hpp"
#include "database/objects/ReleaseTypeId.hpp"

namespace lms::db
{
    struct Filters
    {
        MediaLibraryId mediaLibrary;     // tracks that belongs to this library
        std::vector<ClusterId> clusters; // tracks that belong to all these clusters
        LabelId label;                   // tracks which release has this label
        ReleaseTypeId releaseType;       // tracks which release has this type

        Filters& setClusters(std::span<const ClusterId> _clusters)
        {
            clusters.assign(std::cbegin(_clusters), std::cend(_clusters));
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
    };
} // namespace lms::db