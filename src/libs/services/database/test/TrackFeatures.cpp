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

#include "services/database/TrackFeatures.hpp"

using ScopedTrackFeatures = ScopedEntity<Database::TrackFeatures>;

using namespace Database;

TEST_F(DatabaseFixture, TrackFeatures)
{
	ScopedTrack track {session, "MyTrack"};
	ScopedUser user {session, "MyUser"};

	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_EQ(TrackFeatures::getCount(session), 0);
	}

	ScopedTrackFeatures trackFeatures {session, track.lockAndGet(), ""};

	{
		auto transaction {session.createUniqueTransaction()};
		EXPECT_EQ(TrackFeatures::getCount(session), 1);

		auto allTrackFeatures {TrackFeatures::find(session, Range {})};
		ASSERT_EQ(allTrackFeatures.results.size(), 1);
		EXPECT_EQ(allTrackFeatures.results.front(), trackFeatures.getId());
	}
}
