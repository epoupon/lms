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

#include "services/feedback/IFeedbackService.hpp"

namespace Database
{
	class Db;
}

namespace Feedback
{
	class FeedbackService : public IFeedbackService
	{
		public:
			FeedbackService(Database::Db& db);

		private:
			void star(Database::UserId userId, Database::ArtistId artistId) override;
			void unstar(Database::UserId userId, Database::ArtistId artistId) override;
			bool isStarred(Database::UserId userId, Database::ArtistId artistId) override;

			void star(Database::UserId userId, Database::ReleaseId releaseId) override;
			void unstar(Database::UserId userId, Database::ReleaseId releaseId) override;
			bool isStarred(Database::UserId userId, Database::ReleaseId artistId) override;

			void star(Database::UserId userId, Database::TrackId trackId) override;
			void unstar(Database::UserId userId, Database::TrackId trackId) override;
			bool isStarred(Database::UserId userId, Database::TrackId trackId) override;

			template <typename DatabaseId>
			void starObject(Database::UserId userId, DatabaseId objectId);
			template <typename DatabaseId>
			void unstarObject(Database::UserId userId, DatabaseId objectId);
			template <typename DatabaseId>
			bool isObjectStarred(Database::UserId userId, DatabaseId objectId);

			Database::Db& _db;
	};
} // ns Feedback

