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

#include "Common.hpp"

#include "services/database/TrackBookmark.hpp"

using ScopedTrackBookmark = ScopedEntity<Database::TrackBookmark>;

using namespace Database;

TEST_F(DatabaseFixture, TrackBookmark)
{
	ScopedTrack track {session, "MyTrack"};
	ScopedUser user {session, "MyUser"};

	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_EQ(TrackBookmark::getCount(session), 0);
	}

	ScopedTrackBookmark bookmark {session, user.lockAndGet(), track.lockAndGet()};

	{
		auto transaction {session.createUniqueTransaction()};

		bookmark.get().modify()->setComment("MyComment");
		bookmark.get().modify()->setOffset(std::chrono::milliseconds {5});
	}

	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_EQ(TrackBookmark::getCount(session), 1);

		const auto bookmarks {TrackBookmark::find(session, user.getId(), Range {})};
		ASSERT_EQ(bookmarks.results.size(), 1);
		EXPECT_EQ(bookmarks.results.front(), bookmark.getId());
	}
	{
		auto transaction {session.createSharedTransaction()};

		auto userBookmark {TrackBookmark::find(session, user.getId(), track.getId())};
		ASSERT_TRUE(userBookmark);
		EXPECT_EQ(userBookmark, bookmark.get());

		EXPECT_EQ(userBookmark->getOffset(), std::chrono::milliseconds {5});
		EXPECT_EQ(userBookmark->getComment(), "MyComment");
	}
}
