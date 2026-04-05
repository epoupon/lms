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

#include <numeric>

#include "Common.hpp"

#include "database/objects/TrackAudioFeatures.hpp"

namespace lms::db::tests
{
    using ScopedTrackAudioFeatures = ScopedEntity<db::TrackAudioFeatures>;

    TEST_F(DatabaseFixture, TrackAudioFeatures)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(TrackAudioFeatures ::getCount(session), 0);
        }

        ScopedTrackAudioFeatures trackFeatures{ session, track.lockAndGet() };

        {
            auto transaction{ session.createWriteTransaction() };
            EXPECT_EQ(TrackAudioFeatures ::getCount(session), 1);

            auto allTrackAudioFeatures{ TrackAudioFeatures ::find(session) };
            ASSERT_EQ(allTrackAudioFeatures.results.size(), 1);
            EXPECT_EQ(allTrackAudioFeatures.results.front(), trackFeatures.getId());
        }
    }

    TEST_F(DatabaseFixture, TrackAudioFeatures_data)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedTrackAudioFeatures trackFeatures{ session, track.lockAndGet() };

        using DataType = unsigned char;
        std::vector<DataType> features;
        features.resize(8);
        std::iota(features.begin(), features.end(), 0);

        {
            auto transaction{ session.createWriteTransaction() };
            trackFeatures.get().modify()->setData(std::as_bytes(std::span<const DataType>(features)));
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto fetchedTrackFeatures{ trackFeatures.get() };
            const std::span<const std::byte> rawData{ fetchedTrackFeatures->getData() };
            ASSERT_EQ(rawData.size(), features.size());
            std::span<const DataType> data{ reinterpret_cast<const DataType*>(rawData.data()), rawData.size() };

            ASSERT_EQ(data.size(), features.size());
            for (std::size_t i{}; i < data.size(); ++i)
                EXPECT_EQ(data[i], features[i]) << "Failed at i = " << i;
        }
    }

    TEST_F(DatabaseFixture, TrackAudioFeatures_find)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };

        {
            auto transaction{ session.createReadTransaction() };

            Track::FindParameters params;
            params.setHasAudioFeatures(false);

            bool visited{};
            Track::find(session, params, [&](const Track::pointer& foundTrack) {
                visited = true;
                EXPECT_EQ(foundTrack->getId(), track.getId());
            });
            EXPECT_TRUE(visited);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Track::FindParameters params;
            params.setHasAudioFeatures(true);

            bool visited{};
            Track::find(session, params, [&](const Track::pointer&) {
                visited = true;
            });
            EXPECT_FALSE(visited);
        }

        ScopedTrackAudioFeatures trackFeatures{ session, track.lockAndGet() };

        {
            auto transaction{ session.createReadTransaction() };

            Track::FindParameters params;
            params.setHasAudioFeatures(true);

            bool visited{};
            Track::find(session, params, [&](const Track::pointer& foundTrack) {
                visited = true;
                EXPECT_EQ(foundTrack->getId(), track.getId());
            });
            EXPECT_TRUE(visited);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Track::FindParameters params;
            params.setHasAudioFeatures(false);

            bool visited{};
            Track::find(session, params, [&](const Track::pointer&) {
                visited = true;
            });
            EXPECT_FALSE(visited);
        }
    }
} // namespace lms::db::tests