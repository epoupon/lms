/*
 * Copyright (C) 2025 Emeric Poupon
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
#include "database/Types.hpp"

#include "database/objects/TrackArtistLink.hpp"

namespace lms::db::tests
{
    TEST_F(DatabaseFixture, TrackArtistLink_findArtistNameNoLongerMatch)
    {
        ScopedTrack track{ session };
        ScopedArtist artist{ session, "MyArtist" };

        {
            auto transaction{ session.createWriteTransaction() };
            auto link{ session.create<TrackArtistLink>(track.get(), artist.get(), TrackArtistLinkType::Artist, false) };
            link.modify()->setArtistName("MyArtist");
        }

        {
            auto transaction{ session.createReadTransaction() };

            bool visited{};
            TrackArtistLink::findArtistNameNoLongerMatch(session, std::nullopt, [&](const TrackArtistLink::pointer&) {
                visited = true;
            });
            ASSERT_FALSE(visited);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            artist.get().modify()->setName("MyArtist2");
        }

        {
            auto transaction{ session.createReadTransaction() };

            bool visited{};
            TrackArtistLink::findArtistNameNoLongerMatch(session, std::nullopt, [&](const TrackArtistLink::pointer&) {
                visited = true;
            });
            ASSERT_TRUE(visited);
        }
    }

    TEST_F(DatabaseFixture, TrackArtistLink_findWithArtistNameAmbiguity_split)
    {
        ScopedTrack track{ session };
        ScopedArtist artist1{ session, "MyArtist", core::UUID::fromString("b227426f-98b8-4b39-b3a7-ff25e7711e9b") };

        {
            auto transaction{ session.createWriteTransaction() };
            auto link{ session.create<TrackArtistLink>(track.get(), artist1.get(), TrackArtistLinkType::Artist, false) };
            link.modify()->setArtistName("MyArtist");
        }

        {
            auto transaction{ session.createReadTransaction() };

            bool visited{};
            TrackArtistLink::findWithArtistNameAmbiguity(session, std::nullopt, true /*allow fallback*/, [&](const TrackArtistLink::pointer&) {
                visited = true;
            });
            ASSERT_FALSE(visited);
        }

        {
            auto transaction{ session.createReadTransaction() };

            bool visited{};
            TrackArtistLink::findWithArtistNameAmbiguity(session, std::nullopt, false /*allow fallback*/, [&](const TrackArtistLink::pointer&) {
                visited = true;
            });
            ASSERT_TRUE(visited);
        }

        ScopedArtist artist2{ session, "MyArtist", core::UUID::fromString("97d1fb6f-db09-4760-b0b3-816559bcb632") };
        {
            auto transaction{ session.createReadTransaction() };

            bool visited{};
            TrackArtistLink::findWithArtistNameAmbiguity(session, std::nullopt, true /*allow fallback*/, [&](const TrackArtistLink::pointer&) {
                visited = true;
            });
            ASSERT_TRUE(visited);
        }
        {
            auto transaction{ session.createReadTransaction() };

            bool visited{};
            TrackArtistLink::findWithArtistNameAmbiguity(session, std::nullopt, false /*allow fallback*/, [&](const TrackArtistLink::pointer&) {
                visited = true;
            });
            ASSERT_TRUE(visited);
        }
    }

    TEST_F(DatabaseFixture, TrackArtistLink_findWithArtistNameAmbiguity_merge)
    {
        ScopedTrack track{ session };
        ScopedArtist artist1{ session, "MyArtist" };

        {
            auto transaction{ session.createWriteTransaction() };
            auto link{ session.create<TrackArtistLink>(track.get(), artist1.get(), TrackArtistLinkType::Artist, false) };
            link.modify()->setArtistName("MyArtist");
        }

        {
            auto transaction{ session.createReadTransaction() };

            {
                bool visited{};
                TrackArtistLink::findWithArtistNameAmbiguity(session, std::nullopt, true /*allow fallback*/, [&](const TrackArtistLink::pointer&) {
                    visited = false;
                });
                ASSERT_FALSE(visited);
            }
            {
                bool visited{};
                TrackArtistLink::findWithArtistNameAmbiguity(session, std::nullopt, false /*allow fallback*/, [&](const TrackArtistLink::pointer&) {
                    visited = true;
                });
                ASSERT_FALSE(visited);
            }
        }

        ScopedArtist artist2{ session, "MyArtist", core::UUID::fromString("97d1fb6f-db09-4760-b0b3-816559bcb632") };
        {
            auto transaction{ session.createReadTransaction() };

            {
                bool visited{};
                TrackArtistLink::findWithArtistNameAmbiguity(session, std::nullopt, true /*allow fallback*/, [&](const TrackArtistLink::pointer&) {
                    visited = true;
                });
                ASSERT_TRUE(visited);
            }
            {
                bool visited{};
                TrackArtistLink::findWithArtistNameAmbiguity(session, std::nullopt, false /*allow fallback*/, [&](const TrackArtistLink::pointer&) {
                    visited = true;
                });
                ASSERT_FALSE(visited);
            }
        }

        ScopedArtist artist3{ session, "MyArtist", core::UUID::fromString("3d46c4fb-110d-4d4f-a2d5-5ca57ef1d582") };
        {
            auto transaction{ session.createReadTransaction() };

            {
                bool visited{};
                TrackArtistLink::findWithArtistNameAmbiguity(session, std::nullopt, true /*allow fallback*/, [&](const TrackArtistLink::pointer&) {
                    visited = true;
                });
                ASSERT_FALSE(visited);
            }
            {
                bool visited{};
                TrackArtistLink::findWithArtistNameAmbiguity(session, std::nullopt, false /*allow fallback*/, [&](const TrackArtistLink::pointer&) {
                    visited = true;
                });
                ASSERT_FALSE(visited);
            }
        }
    }
} // namespace lms::db::tests