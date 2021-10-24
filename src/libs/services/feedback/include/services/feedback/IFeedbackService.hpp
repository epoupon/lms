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

#include "services/database/Types.hpp"

namespace Database
{
	class Db;
}

namespace Feedback
{
	class IFeedbackService
	{
		public:
			virtual ~IFeedbackService() = default;

			virtual void star(Database::UserId userId, Database::ArtistId artistId) = 0;
			virtual void unstar(Database::UserId userId, Database::ArtistId artistId) = 0;
			virtual bool isStarred(Database::UserId userId, Database::ArtistId artistId) = 0;

			virtual void star(Database::UserId userId, Database::ReleaseId releaseId) = 0;
			virtual void unstar(Database::UserId userId, Database::ReleaseId releaseId) = 0;
			virtual bool isStarred(Database::UserId userId, Database::ReleaseId artistId) = 0;

			virtual void star(Database::UserId userId, Database::TrackId trackId) = 0;
			virtual void unstar(Database::UserId userId, Database::TrackId trackId) = 0;
			virtual bool isStarred(Database::UserId userId, Database::TrackId artistId) = 0;
	};

	std::unique_ptr<IFeedbackService> createFeedbackService(Database::Db& db);
} // ns Feedback

