/*
 * Copyright (C) 2022 Emeric Poupon
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

#include "database/objects/ReleaseId.hpp"
#include "database/objects/TrackId.hpp"

namespace lms::ui
{
    class PlayQueue;

    // Used to interact with the play queue: callers give what they show, releases are played whole
    class PlayQueueController
    {
    public:
        explicit PlayQueueController(PlayQueue& playQueue);
        ~PlayQueueController() = default;
        PlayQueueController(const PlayQueueController&) = delete;
        PlayQueueController& operator=(const PlayQueueController&) = delete;

        enum class Command
        {
            Play,
            PlayNext,
            PlayOrAddLast,
            PlayShuffled,
        };

        void processCommand(Command command, std::span<const db::ReleaseId> releases);
        void processCommand(Command command, std::span<const db::TrackId> tracks);

        void playAtIndex(std::span<const db::TrackId> trackIds, std::size_t index);

    private:
        PlayQueue& _playQueue;
    };
} // namespace lms::ui
