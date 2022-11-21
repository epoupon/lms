/*
 * Copyright (C) 2020 Emeric Poupon
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

#include <memory>

#include <Wt/WWidget.h>
#include "services/database/Object.hpp"
#include "services/database/TrackId.hpp"

namespace Database
{
	class Track;
}

namespace UserInterface
{
	class PlayQueueController;
	class Filters;
}

namespace UserInterface::TrackListHelpers
{
	void showTrackInfoModal(Database::TrackId trackId, Filters& filters);
	std::unique_ptr<Wt::WWidget> createEntry(const Database::ObjectPtr<Database::Track>& track, PlayQueueController& playQueueController, Filters& filters);
} // namespace UserInterface

