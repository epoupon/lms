/*
 * Copyright (C) 2021 Emeric Poupon
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
#include <memory>
#include <optional>

#include "services/database/StarredArtistId.hpp"
#include "services/database/StarredReleaseId.hpp"
#include "services/database/StarredTrackId.hpp"
#include "services/scrobbling/Listen.hpp"

namespace Database
{
    class Session;
    class TrackList;
    class User;
}

namespace Scrobbling
{
    class IScrobbler
    {
    public:
        virtual ~IScrobbler() = default;

        // Listens
        virtual void listenStarted(const Listen& listen) = 0;
        virtual void listenFinished(const Listen& listen, std::optional<std::chrono::seconds> duration) = 0;
        virtual void addTimedListen(const TimedListen& listen) = 0;

        // Feedbacks
        virtual void onStarred(Database::StarredArtistId) = 0;
        virtual void onUnstarred(Database::StarredArtistId) = 0;
        virtual void onStarred(Database::StarredReleaseId) = 0;
        virtual void onUnstarred(Database::StarredReleaseId) = 0;
        virtual void onStarred(Database::StarredTrackId) = 0;
        virtual void onUnstarred(Database::StarredTrackId) = 0;
    };

    std::unique_ptr<IScrobbler> createScrobbler(std::string_view backendName);

} // ns Scrobbling

