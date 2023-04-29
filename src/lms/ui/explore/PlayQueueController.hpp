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

#include <vector>

#include "services/database/ArtistId.hpp"
#include "services/database/ReleaseId.hpp"
#include "services/database/TrackId.hpp"
#include "services/database/TrackListId.hpp"

namespace UserInterface
{
	class Filters;
	class PlayQueue;

	// Used to interact with the play queue, using the current exploration filters
	class PlayQueueController
	{
		public:
			PlayQueueController(Filters& filters, PlayQueue& playQueue);

			enum class Command
			{
				Play,
				PlayNext,
				PlayOrAddLast,
				PlayShuffled,
			};

			void processCommand(Command command, const std::vector<Database::ArtistId>&);
			void processCommand(Command command, const std::vector<Database::ReleaseId>&);
			void processCommand(Command command, const std::vector<Database::TrackId>&);
			void processCommand(Command command, Database::TrackListId);
			void playTrackInRelease(Database::TrackId);

			void setMaxTrackCountToEnqueue(std::size_t maxTrackCount) { _maxTrackCountToEnqueue = maxTrackCount; }

		private:
			Filters& _filters;
			PlayQueue& _playQueue;
			std::size_t _maxTrackCountToEnqueue {};
	};
}

