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

#include <chrono>
#include <string>

#include <Wt/WImage.h>
#include <Wt/WInteractWidget.h>

#include "services/database/ClusterId.hpp"
#include "services/database/ReleaseId.hpp"
#include "services/database/TrackId.hpp"

#include "resource/CoverResource.hpp"

namespace UserInterface::Utils
{
	std::string
	durationToString(std::chrono::milliseconds msDuration);

	std::unique_ptr<Wt::WImage> createCover(Database::ReleaseId releaseId, CoverResource::Size size);
	std::unique_ptr<Wt::WImage> createCover(Database::TrackId trackId, CoverResource::Size size);

	std::unique_ptr<Wt::WInteractWidget> createCluster(Database::ClusterId clusterId, bool canDelete = false);
}
