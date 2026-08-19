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

    TEST_F(DatabaseFixture, TrackArtistLink_findWithOriginalDateDesc)
    {
        ScopedArtist artist{ session, "MyArtist" };
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };

        {
            auto transaction{ session.createWriteTransaction() };

            {
                auto link1{ session.create<TrackArtistLink>(track1.get(), artist.get(), TrackArtistLinkType::Artist, false) };
                link1.modify()->setArtistName("MyArtistOldName");
                track1.get().modify()->setOriginalDate(core::PartialDateTime{ 1990, 1 });
            }

            {
                auto link2{ session.create<TrackArtistLink>(track2.get(), artist.get(), TrackArtistLinkType::Artist, false) };
                link2.modify()->setArtistName("MyArtistNewName");
                track2.get().modify()->setOriginalDate(core::PartialDateTime{ 1995, 1 });
            }
        }

        {
            auto transaction{ session.createReadTransaction() };

            TrackArtistLink::FindParameters params;
            params.setSortMethod(TrackArtistLinkSortMethod::OriginalDateDesc);
            params.setArtist(artist->getId());

            std::vector<TrackArtistLink::pointer> links;
            TrackArtistLink::find(session, params, [&](const TrackArtistLink::pointer& link) {
                links.push_back(link);
            });
            ASSERT_EQ(links.size(), 2);
            EXPECT_EQ(links[0]->getArtistName(), "MyArtistNewName");
            EXPECT_EQ(links[1]->getArtistName(), "MyArtistOldName");
        }
    }

    TEST_F(DatabaseFixture, TrackArtistLink_findWithMBIDMatched)
    {
        ScopedArtist artist{ session, "MyArtist", core::UUID::fromString("97d1fb6f-db09-4760-b0b3-816559bcb632") };
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            auto link1{ session.create<TrackArtistLink>(track1.get(), artist.get(), TrackArtistLinkType::Artist, false) };
            auto link2{ session.create<TrackArtistLink>(track2.get(), artist.get(), TrackArtistLinkType::Artist, true) };
        }

        {
            auto transaction{ session.createReadTransaction() };

            TrackArtistLink::FindParameters params;

            std::vector<TrackArtistLink::pointer> links;
            TrackArtistLink::find(session, params, [&](const TrackArtistLink::pointer& link) {
                links.push_back(link);
            });
            ASSERT_EQ(links.size(), 2);
        }

        {
            auto transaction{ session.createReadTransaction() };

            TrackArtistLink::FindParameters params;
            params.setMBIDMatched(false);

            std::vector<TrackArtistLink::pointer> links;
            TrackArtistLink::find(session, params, [&](const TrackArtistLink::pointer& link) {
                links.push_back(link);
            });
            ASSERT_EQ(links.size(), 1);
            EXPECT_EQ(links[0]->getTrack()->getId(), track1.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            TrackArtistLink::FindParameters params;
            params.setMBIDMatched(true);

            std::vector<TrackArtistLink::pointer> links;
            TrackArtistLink::find(session, params, [&](const TrackArtistLink::pointer& link) {
                links.push_back(link);
            });
            ASSERT_EQ(links.size(), 1);
            EXPECT_EQ(links[0]->getTrack()->getId(), track2.getId());
        }
    }

    TEST_F(DatabaseFixture, Track_getArtists_typeFilterAndOrder)
    {
        ScopedTrack track{ session };
        ScopedArtist artist1{ session, "Artist1" };
        ScopedArtist artist2{ session, "Artist2" };

        {
            auto transaction{ session.createWriteTransaction() };
            session.create<TrackArtistLink>(track.get(), artist1.get(), TrackArtistLinkType::Artist, false);
            session.create<TrackArtistLink>(track.get(), artist2.get(), TrackArtistLinkType::Artist, false);
            session.create<TrackArtistLink>(track.get(), artist1.get(), TrackArtistLinkType::Mixer, false);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto artistLinks{ track->getArtists({ TrackArtistLinkType::Artist }) };
            ASSERT_EQ(artistLinks.size(), 2);
            EXPECT_EQ(artistLinks[0]->getId(), artist1.getId());
            EXPECT_EQ(artistLinks[1]->getId(), artist2.getId());

            const auto mixerLinks{ track->getArtists({ TrackArtistLinkType::Mixer }) };
            ASSERT_EQ(mixerLinks.size(), 1);
            EXPECT_EQ(mixerLinks[0]->getId(), artist1.getId());

            const auto noFilter{ track->getArtists({}) };
            EXPECT_EQ(noFilter.size(), 2);
        }
    }

    TEST_F(DatabaseFixture, Track_getArtistIds_typeFilter)
    {
        ScopedTrack track{ session };
        ScopedArtist artist1{ session, "Artist1" };
        ScopedArtist artist2{ session, "Artist2" };

        {
            auto transaction{ session.createWriteTransaction() };
            session.create<TrackArtistLink>(track.get(), artist1.get(), TrackArtistLinkType::Artist, false);
            session.create<TrackArtistLink>(track.get(), artist2.get(), TrackArtistLinkType::Artist, false);
            session.create<TrackArtistLink>(track.get(), artist1.get(), TrackArtistLinkType::Mixer, false);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto artistIds{ track->getArtistIds({ TrackArtistLinkType::Artist }) };
            ASSERT_EQ(artistIds.size(), 2);
            EXPECT_EQ(artistIds[0], artist1.getId());
            EXPECT_EQ(artistIds[1], artist2.getId());

            const auto mixerIds{ track->getArtistIds({ TrackArtistLinkType::Mixer }) };
            ASSERT_EQ(mixerIds.size(), 1);
            EXPECT_EQ(mixerIds[0], artist1.getId());

            const auto noFilter{ track->getArtistIds({}) };
            EXPECT_EQ(noFilter.size(), 2);
        }
    }

    TEST_F(DatabaseFixture, Track_visitArtistLinks_orderedById)
    {
        ScopedTrack track{ session };
        ScopedArtist artist1{ session, "Artist1" };
        ScopedArtist artist2{ session, "Artist2" };

        TrackArtistLinkId link1Id;

        {
            auto transaction{ session.createWriteTransaction() };
            auto link1{ session.create<TrackArtistLink>(track.get(), artist1.get(), TrackArtistLinkType::Artist, false) };
            link1Id = link1->getId();
            session.create<TrackArtistLink>(track.get(), artist2.get(), TrackArtistLinkType::Artist, false);
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::vector<db::TrackArtistLink::pointer> links;
            track->visitArtistLinks([&](const db::TrackArtistLink::pointer& link) {
                links.push_back(link);
            });

            ASSERT_EQ(links.size(), 2);
            EXPECT_EQ(links[0]->getArtistId(), artist1.getId());
            EXPECT_EQ(links[1]->getArtistId(), artist2.getId());
        }

        {
            auto transaction{ session.createWriteTransaction() };
            TrackArtistLink::find(session, link1Id).remove();
        }

        {
            auto transaction{ session.createWriteTransaction() };
            session.create<TrackArtistLink>(track.get(), artist1.get(), TrackArtistLinkType::Artist, false);
        }

        // artist2's link has lower id, artist1's re-created link has higher id.
        {
            auto transaction{ session.createReadTransaction() };

            std::vector<db::TrackArtistLink::pointer> links;
            track->visitArtistLinks([&](const db::TrackArtistLink::pointer& link) {
                links.push_back(link);
            });

            ASSERT_EQ(links.size(), 2);
            EXPECT_EQ(links[0]->getArtistId(), artist2.getId());
            EXPECT_EQ(links[1]->getArtistId(), artist1.getId());
        }
    }

} // namespace lms::db::tests