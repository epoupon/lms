/*
 * Copyright (C) 2025 Emeric Poupon
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

#include "database/objects/Movement.hpp"

#include <Wt/Dbo/Impl.h>

#include "core/ILogger.hpp"
#include "core/String.hpp"

#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/Genre.hpp"
#include "database/objects/Grouping.hpp"
#include "database/objects/Language.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "database/objects/Medium.hpp"
#include "database/objects/Mood.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/TrackEmbeddedImage.hpp"
#include "database/objects/TrackEmbeddedImageLink.hpp"
#include "database/objects/TrackLyrics.hpp"

#include "traits/IdTypeTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::Movement)

namespace lms::db
{
    Movement::Movement(std::string_view name, std::optional<std::size_t> number, std::optional<std::size_t> count, const ObjectPtr<Track>& track)
        : _name{ core::stringUtils::utf8Truncate(name, maxNameLength) }
        , _number{ number }
        , _count{ count }
        , _track{ getDboPtr(track) }
    {
        LMS_LOG_IF(DB, WARNING, name.size() > maxNameLength, "Movement name too long, truncated to '" << _name << "'");
    }

    Movement::pointer Movement::create(Session& session, std::string_view name, std::optional<std::size_t> number, std::optional<std::size_t> count, const ObjectPtr<Track>& track)
    {
        return session.getDboSession()->add(std::unique_ptr<Movement>{ new Movement{ name, number, count, track } });
    }

} // namespace lms::db
