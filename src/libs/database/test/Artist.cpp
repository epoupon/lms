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

#include "database/Image.hpp"

namespace lms::db::tests
{
    using ScopedImage = ScopedEntity<db::Image>;

    TEST_F(DatabaseFixture, Artist)
    {
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_FALSE(Artist::exists(session, 35));
            EXPECT_FALSE(Artist::exists(session, 0));
            EXPECT_FALSE(Artist::exists(session, 1));
            EXPECT_EQ(Artist::getCount(session), 0);
        }

        ScopedArtist artist{ session, "MyArtist" };

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_TRUE(artist.get());
            EXPECT_FALSE(!artist.get());
            EXPECT_EQ(artist.get()->getId(), artist.getId());

            EXPECT_TRUE(Artist::exists(session, artist.getId()));
            EXPECT_EQ(Artist::getCount(session), 1);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ Artist::findIds(session, Artist::FindParameters{}) };
            ASSERT_EQ(artists.results.size(), 1);
            EXPECT_EQ(artists.results.front(), artist.getId());

            artists = Artist::findOrphanIds(session);
            ASSERT_EQ(artists.results.size(), 1);
            EXPECT_EQ(artists.results.front(), artist.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ Artist::find(session, Artist::FindParameters{}) };
            ASSERT_EQ(artists.results.size(), 1);
            EXPECT_EQ(artists.results.front()->getId(), artist.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            bool visited{};
            Artist::find(session, Artist::FindParameters{}, [&](const Artist::pointer& a) {
                visited = true;
                EXPECT_EQ(a->getId(), artist.getId());
            });
            EXPECT_TRUE(visited);
        }
    }

    TEST_F(DatabaseFixture, Artist_findByRangedIdBased)
    {
        ScopedTrack track1{ session };
        ScopedTrack track2a{ session };
        ScopedTrack track2b{ session };
        ScopedTrack track3{ session };
        ScopedArtist artist1{ session, "MyArtist1" };
        ScopedArtist artist2{ session, "MyArtist2" };
        ScopedArtist artist3{ session, "MyArtist3" };
        ScopedMediaLibrary library{ session };
        ScopedMediaLibrary otherLibrary{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            track2a.get().modify()->setMediaLibrary(library.get());
            track2b.get().modify()->setMediaLibrary(library.get());
            TrackArtistLink::create(session, track1.get(), artist1.get(), TrackArtistLinkType::Artist);
            TrackArtistLink::create(session, track2a.get(), artist2.get(), TrackArtistLinkType::Artist);
            TrackArtistLink::create(session, track2b.get(), artist2.get(), TrackArtistLinkType::Artist);
            TrackArtistLink::create(session, track3.get(), artist3.get(), TrackArtistLinkType::Artist);
        }

        {
            auto transaction{ session.createReadTransaction() };

            ArtistId lastRetrievedId;
            std::vector<Artist::pointer> visitedArtists;
            Artist::find(session, lastRetrievedId, 10, [&](const Artist::pointer& artist) {
                visitedArtists.push_back(artist);
            });
            ASSERT_EQ(visitedArtists.size(), 3);
            EXPECT_EQ(visitedArtists[0]->getId(), artist1.getId());
            EXPECT_EQ(visitedArtists[1]->getId(), artist2.getId());
            EXPECT_EQ(visitedArtists[2]->getId(), artist3.getId());
            EXPECT_EQ(lastRetrievedId, artist3.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            ArtistId lastRetrievedId{ artist1.getId() };
            std::vector<Artist::pointer> visitedArtists;
            Artist::find(session, lastRetrievedId, 1, [&](const Artist::pointer& artist) {
                visitedArtists.push_back(artist);
            });
            ASSERT_EQ(visitedArtists.size(), 1);
            EXPECT_EQ(visitedArtists[0]->getId(), artist2.getId());
            EXPECT_EQ(lastRetrievedId, artist2.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            ArtistId lastRetrievedId{ artist1.getId() };
            std::vector<Artist::pointer> visitedArtists;
            Artist::find(session, lastRetrievedId, 0, [&](const Artist::pointer& artist) {
                visitedArtists.push_back(artist);
            });
            ASSERT_EQ(visitedArtists.size(), 0);
            EXPECT_EQ(lastRetrievedId, artist1.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            ArtistId lastRetrievedId;
            std::vector<Artist::pointer> visitedArtists;
            Artist::find(
                session, lastRetrievedId, 10, [&](const Artist::pointer& artist) {
                    visitedArtists.push_back(artist);
                },
                otherLibrary.getId());
            ASSERT_EQ(visitedArtists.size(), 0);
            EXPECT_EQ(lastRetrievedId, ArtistId{});
        }

        {
            auto transaction{ session.createReadTransaction() };

            ArtistId lastRetrievedId;
            std::vector<Artist::pointer> visitedArtists;
            Artist::find(
                session, lastRetrievedId, 10, [&](const Artist::pointer& artist) {
                    visitedArtists.push_back(artist);
                },
                library.getId());
            ASSERT_EQ(visitedArtists.size(), 1);
            EXPECT_EQ(visitedArtists[0]->getId(), artist2.getId());
            EXPECT_EQ(lastRetrievedId, artist2.getId());
        }
    }

    TEST_F(DatabaseFixture, MultipleArtists)
    {
        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ Artist::findIds(session, Artist::FindParameters{}) };
            ASSERT_EQ(artists.results.size(), 0);
            ASSERT_FALSE(artists.moreResults);
            ASSERT_EQ(artists.range.offset, 0);
            ASSERT_EQ(artists.range.size, 0);
        }

        ScopedArtist artist1{ session, "MyArtist1" };
        ScopedArtist artist2{ session, "MyArtist2" };
        ScopedArtist artist3{ session, "MyArtist3" };

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ Artist::findIds(session, Artist::FindParameters{}) };
            ASSERT_EQ(artists.results.size(), 3);
            ASSERT_FALSE(artists.moreResults);
            ASSERT_EQ(artists.range.offset, 0);
            ASSERT_EQ(artists.range.size, 3);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setRange(Range{ 0, 1 })) };
            ASSERT_EQ(artists.results.size(), 1);
            ASSERT_TRUE(artists.moreResults);
            ASSERT_EQ(artists.range.offset, 0);
            ASSERT_EQ(artists.range.size, 1);
            EXPECT_EQ(artists.results[0], artist1.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setRange(Range{ 1, 1 })) };
            ASSERT_EQ(artists.results.size(), 1);
            ASSERT_TRUE(artists.moreResults);
            ASSERT_EQ(artists.range.offset, 1);
            ASSERT_EQ(artists.range.size, 1);
            EXPECT_EQ(artists.results[0], artist2.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setRange(Range{ 2, 1 })) };
            ASSERT_EQ(artists.results.size(), 1);
            ASSERT_FALSE(artists.moreResults);
            ASSERT_EQ(artists.range.offset, 2);
            ASSERT_EQ(artists.range.size, 1);
            EXPECT_EQ(artists.results[0], artist3.getId());
        }
    }

    TEST_F(DatabaseFixture, Artist_singleTrack)
    {
        ScopedTrack track{ session };
        ScopedArtist artist{ session, "MyArtist" };

        {
            auto transaction{ session.createWriteTransaction() };

            track.get().modify()->setName("MyTrackName");
            TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Artist::findOrphanIds(session).results.size(), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ track->getArtists({ TrackArtistLinkType::Artist }) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front()->getId(), artist.getId());

            ASSERT_EQ(track->getArtistLinks().size(), 1);
            auto artistLink{ track->getArtistLinks().front() };
            EXPECT_EQ(artistLink->getTrack()->getId(), track.getId());
            EXPECT_EQ(artistLink->getArtist()->getId(), artist.getId());

            ASSERT_EQ(track->getArtists({ TrackArtistLinkType::Artist }).size(), 1);
            EXPECT_EQ(track->getArtists({ TrackArtistLinkType::ReleaseArtist }).size(), 0);
            EXPECT_EQ(track->getArtists({}).size(), 1);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ track->getArtistIds({ TrackArtistLinkType::Artist }) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front(), artist.getId());

            ASSERT_EQ(track->getArtistIds({ TrackArtistLinkType::Artist }).size(), 1);
            EXPECT_EQ(track->getArtistIds({ TrackArtistLinkType::ReleaseArtist }).size(), 0);
            EXPECT_EQ(track->getArtistIds({}).size(), 1);
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto tracks{ Track::findIds(session, Track::FindParameters{}.setName("MyTrackName").setArtistName("MyArtist")) };
            ASSERT_EQ(tracks.results.size(), 1);
            EXPECT_EQ(tracks.results.front(), track.getId());
        }
        {
            auto transaction{ session.createReadTransaction() };
            auto tracks{ Track::findIds(session, Track::FindParameters{}.setName("MyTrackName").setArtistName("MyArtistFoo")) };
            EXPECT_EQ(tracks.results.size(), 0);
        }
        {
            auto transaction{ session.createReadTransaction() };
            auto tracks{ Track::findIds(session, Track::FindParameters{}.setName("MyTrackNameFoo").setArtistName("MyArtist")) };
            EXPECT_EQ(tracks.results.size(), 0);
        }
        {
            auto transaction{ session.createReadTransaction() };
            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setTrack(track->getId())) };
            ASSERT_EQ(artists.results.size(), 1);
            EXPECT_EQ(artists.results.front(), artist.getId());
        }
    }

    TEST_F(DatabaseFixture, Artist_singleTrack_mediaLibrary)
    {
        ScopedTrack track{ session };
        ScopedArtist artist{ session, "MyArtist" };
        ScopedMediaLibrary library{ session };
        ScopedMediaLibrary otherLibrary{ session };

        {
            auto transaction{ session.createWriteTransaction() };

            track.get().modify()->setName("MyTrackName");
            TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
            track.get().modify()->setMediaLibrary(library.get());
        }
        {
            auto transaction{ session.createReadTransaction() };
            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setTrack(track->getId())) };
            ASSERT_EQ(artists.results.size(), 1);
            EXPECT_EQ(artists.results.front(), artist.getId());
        }
        {
            auto transaction{ session.createReadTransaction() };
            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setMediaLibrary(library->getId())) };
            ASSERT_EQ(artists.results.size(), 1);
            EXPECT_EQ(artists.results.front(), artist.getId());
        }
        {
            auto transaction{ session.createReadTransaction() };
            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setMediaLibrary(otherLibrary->getId())) };
            EXPECT_EQ(artists.results.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Artist_singleTracktMultiRoles)
    {
        ScopedTrack track{ session };
        ScopedArtist artist{ session, "MyArtist" };
        {
            auto transaction{ session.createWriteTransaction() };

            TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
            TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::ReleaseArtist);
            TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Writer);
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Artist::findOrphanIds(session, Range{}).results.size(), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Artist::findIds(session, Artist::FindParameters{}).results.size(), 1);
            EXPECT_EQ(Artist::findIds(session, Artist::FindParameters{}.setLinkType(TrackArtistLinkType::Artist)).results.size(), 1);
            EXPECT_EQ(Artist::findIds(session, Artist::FindParameters{}.setLinkType(TrackArtistLinkType::ReleaseArtist)).results.size(), 1);
            EXPECT_EQ(Artist::findIds(session, Artist::FindParameters{}.setLinkType(TrackArtistLinkType::Writer)).results.size(), 1);
            EXPECT_EQ(Artist::findIds(session, Artist::FindParameters{}.setLinkType(TrackArtistLinkType::Composer)).results.size(), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ track->getArtists({ TrackArtistLinkType::Artist }) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front()->getId(), artist.getId());

            artists = track->getArtists({ TrackArtistLinkType::ReleaseArtist });
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front()->getId(), artist.getId());

            EXPECT_EQ(track->getArtistLinks().size(), 3);

            auto tracks{ Track::findIds(session, Track::FindParameters{}.setArtist(artist.getId())) };
            EXPECT_EQ(tracks.results.size(), 1);

            tracks = Track::findIds(session, Track::FindParameters{}.setArtist(artist.getId(), { TrackArtistLinkType::ReleaseArtist }));
            EXPECT_EQ(tracks.results.size(), 1);
            tracks = Track::findIds(session, Track::FindParameters{}.setArtist(artist.getId(), { TrackArtistLinkType::Artist }));
            EXPECT_EQ(tracks.results.size(), 1);
            tracks = Track::findIds(session, Track::FindParameters{}.setArtist(artist.getId(), { TrackArtistLinkType::Writer }));
            EXPECT_EQ(tracks.results.size(), 1);

            tracks = Track::findIds(session, Track::FindParameters{}.setArtist(artist.getId(), { TrackArtistLinkType::Composer }));
            EXPECT_EQ(tracks.results.size(), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };
            core::EnumSet<TrackArtistLinkType> types{ TrackArtistLink::findUsedTypes(session, artist.getId()) };
            EXPECT_TRUE(types.contains(TrackArtistLinkType::ReleaseArtist));
            EXPECT_TRUE(types.contains(TrackArtistLinkType::Artist));
            EXPECT_TRUE(types.contains(TrackArtistLinkType::Writer));
            EXPECT_FALSE(types.contains(TrackArtistLinkType::Composer));
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::vector<TrackArtistLink::pointer> visitedLinks;
            TrackArtistLink::find(session, TrackArtistLink::FindParameters{}.setTrack(track.getId()), [&](const TrackArtistLink::pointer& link) {
                visitedLinks.push_back(link);
            });
            ASSERT_EQ(visitedLinks.size(), 3);
            EXPECT_EQ(visitedLinks[0]->getArtist()->getId(), artist.getId());
            EXPECT_EQ(visitedLinks[1]->getArtist()->getId(), artist.getId());
            EXPECT_EQ(visitedLinks[2]->getArtist()->getId(), artist.getId());

            auto containsType = [&](TrackArtistLinkType type) {
                return std::any_of(std::cbegin(visitedLinks), std::cend(visitedLinks), [type](const TrackArtistLink::pointer& link) { return link->getType() == type; });
            };

            EXPECT_TRUE(containsType(TrackArtistLinkType::Artist));
            EXPECT_TRUE(containsType(TrackArtistLinkType::ReleaseArtist));
            EXPECT_TRUE(containsType(TrackArtistLinkType::Writer));
        }
    }

    TEST_F(DatabaseFixture, Artist_singleTrackMultiArtists)
    {
        ScopedTrack track{ session };
        ScopedArtist artist1{ session, "artist1" };
        ScopedArtist artist2{ session, "artist2" };
        ASSERT_NE(artist1.getId(), artist2.getId());

        {
            auto transaction{ session.createWriteTransaction() };

            TrackArtistLink::create(session, track.get(), artist1.get(), TrackArtistLinkType::Artist);
            TrackArtistLink::create(session, track.get(), artist2.get(), TrackArtistLinkType::Artist);
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Artist::findOrphanIds(session).results.size(), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ track->getArtists({ TrackArtistLinkType::Artist }) };
            ASSERT_EQ(artists.size(), 2);
            EXPECT_TRUE((artists[0]->getId() == artist1.getId() && artists[1]->getId() == artist2.getId())
                        || (artists[0]->getId() == artist2.getId() && artists[1]->getId() == artist1.getId()));

            EXPECT_EQ(track->getArtists({}).size(), 2);
            EXPECT_EQ(track->getArtists({ TrackArtistLinkType::Artist }).size(), 2);
            EXPECT_EQ(track->getArtists({ TrackArtistLinkType::ReleaseArtist }).size(), 0);
            EXPECT_EQ(Artist::findIds(session, Artist::FindParameters{}).results.size(), 2);
            EXPECT_EQ(Artist::findIds(session, Artist::FindParameters{}.setSortMethod(ArtistSortMethod::Random)).results.size(), 2);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto tracks{ Track::findIds(session, Track::FindParameters{}.setArtist(artist1->getId())) };
            ASSERT_EQ(tracks.results.size(), 1);
            EXPECT_EQ(tracks.results.front(), track->getId());

            tracks = Track::findIds(session, Track::FindParameters{}.setArtist(artist2->getId()));
            ASSERT_EQ(tracks.results.size(), 1);
            EXPECT_EQ(tracks.results.front(), track->getId());

            tracks = Track::findIds(session, Track::FindParameters{}.setArtist(artist1->getId(), { TrackArtistLinkType::ReleaseArtist }));
            EXPECT_EQ(tracks.results.size(), 0);

            tracks = Track::findIds(session, Track::FindParameters{}.setArtist(artist1->getId(), { TrackArtistLinkType::Artist }));
            EXPECT_EQ(tracks.results.size(), 1);

            tracks = Track::findIds(session, Track::FindParameters{}.setArtist(artist2->getId(), { TrackArtistLinkType::ReleaseArtist }));
            EXPECT_EQ(tracks.results.size(), 0);

            tracks = Track::findIds(session, Track::FindParameters{}.setArtist(artist2->getId(), { TrackArtistLinkType::Artist }));
            EXPECT_EQ(tracks.results.size(), 1);
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::vector<TrackArtistLink::pointer> visitedLinks;
            TrackArtistLink::find(session, TrackArtistLink::FindParameters{}.setTrack(track.getId()), [&](const TrackArtistLink::pointer& link) {
                visitedLinks.push_back(link);
            });
            ASSERT_EQ(visitedLinks.size(), 2);
            EXPECT_EQ(visitedLinks[0]->getArtist()->getId(), artist1.getId());
            EXPECT_EQ(visitedLinks[1]->getArtist()->getId(), artist2.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::vector<TrackArtistLink::pointer> visitedLinks;
            TrackArtistLink::find(session, TrackArtistLink::FindParameters{}.setArtist(artist2.getId()), [&](const TrackArtistLink::pointer& link) {
                visitedLinks.push_back(link);
            });
            ASSERT_EQ(visitedLinks.size(), 1);
            EXPECT_EQ(visitedLinks[0]->getArtist()->getId(), artist2.getId());
            EXPECT_EQ(visitedLinks[0]->getTrack()->getId(), track.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::vector<std::pair<TrackArtistLink::pointer, Artist::pointer>> visitedEntries;
            TrackArtistLink::find(session, track.getId(), [&](const TrackArtistLink::pointer& link, const Artist::pointer& artist) {
                visitedEntries.push_back(std::make_pair(link, artist));
            });
            ASSERT_EQ(visitedEntries.size(), 2);
            EXPECT_EQ(visitedEntries[0].first->getArtist()->getId(), artist1.getId());
            EXPECT_EQ(visitedEntries[0].second->getId(), artist1.getId());
            EXPECT_EQ(visitedEntries[1].first->getArtist()->getId(), artist2.getId());
            EXPECT_EQ(visitedEntries[1].second->getId(), artist2.getId());
        }
    }

    TEST_F(DatabaseFixture, Artist_findByName)
    {
        ScopedArtist artist{ session, "AAA" };
        ScopedTrack track{ session }; // filters does not work on orphans

        {
            auto transaction{ session.createWriteTransaction() };
            artist.get().modify()->setSortName("ZZZ");
            TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
        }

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(Artist::findIds(session, Artist::FindParameters{}.setKeywords({ "N" })).results.size(), 0);

            const auto artistsByAAA{ Artist::findIds(session, Artist::FindParameters{}.setKeywords({ "A" })) };
            ASSERT_EQ(artistsByAAA.results.size(), 1);
            EXPECT_EQ(artistsByAAA.results.front(), artist.getId());

            const auto artistsByZZZ{ Artist::Artist::findIds(session, Artist::FindParameters{}.setKeywords({ "Z" })) };
            ASSERT_EQ(artistsByZZZ.results.size(), 1);
            EXPECT_EQ(artistsByZZZ.results.front(), artist.getId());

            EXPECT_EQ(Artist::find(session, "NNN").size(), 0);
            EXPECT_EQ(Artist::find(session, "AAA").size(), 1);
        }
    }

    TEST_F(DatabaseFixture, Artist_findByNameEscaped)
    {
        ScopedArtist artist1{ session, R"(MyArtist%)" };
        ScopedArtist artist2{ session, R"(%MyArtist)" };
        ScopedArtist artist3{ session, R"(%_MyArtist)" };

        ScopedArtist artist4{ session, R"(MyArtist%foo)" };
        ScopedArtist artist5{ session, R"(foo%MyArtist)" };
        ScopedArtist artist6{ session, R"(%AMyArtist)" };

        {
            auto transaction{ session.createReadTransaction() };
            {
                const auto artists{ Artist::find(session, R"(MyArtist%)") };
                ASSERT_TRUE(artists.size() == 1);
                EXPECT_EQ(artists.front()->getId(), artist1.getId());
                EXPECT_EQ(Artist::find(session, R"(MyArtistFoo)").size(), 0);
            }
            {
                const auto artists{ Artist::find(session, R"(%MyArtist)") };
                ASSERT_TRUE(artists.size() == 1);
                EXPECT_EQ(artists.front()->getId(), artist2.getId());
                EXPECT_EQ(Artist::find(session, R"(FooMyArtist)").size(), 0);
            }
            {
                const auto artists{ Artist::find(session, R"(%_MyArtist)") };
                ASSERT_TRUE(artists.size() == 1);
                ASSERT_EQ(artists.front()->getId(), artist3.getId());
                EXPECT_EQ(Artist::find(session, R"(%CMyArtist)").size(), 0);
            }
        }

        {
            auto transaction{ session.createReadTransaction() };
            {
                const auto artists{ Artist::findIds(session, Artist::FindParameters{}.setKeywords({ "MyArtist" })) };
                EXPECT_EQ(artists.results.size(), 6);
            }

            {
                const auto artists{ Artist::findIds(session, Artist::FindParameters{}.setKeywords({ "MyArtist%" }).setSortMethod(ArtistSortMethod::Name)) };
                ASSERT_EQ(artists.results.size(), 2);
                EXPECT_EQ(artists.results[0], artist1.getId());
                EXPECT_EQ(artists.results[1], artist4.getId());
            }

            {
                const auto artists{ Artist::findIds(session, Artist::FindParameters{}.setKeywords({ "%MyArtist" }).setSortMethod(ArtistSortMethod::Name)) };
                ASSERT_EQ(artists.results.size(), 2);
                EXPECT_EQ(artists.results[0], artist2.getId());
                EXPECT_EQ(artists.results[1], artist5.getId());
            }

            {
                const auto artists{ Artist::findIds(session, Artist::FindParameters{}.setKeywords({ "_MyArtist" }).setSortMethod(ArtistSortMethod::Name)) };
                ASSERT_EQ(artists.results.size(), 1);
                EXPECT_EQ(artists.results[0], artist3.getId());
            }
        }
    }

    TEST_F(DatabaseFixture, Artist_sortMethod)
    {
        ScopedArtist artistA{ session, "artistA" };
        ScopedArtist artistB{ session, "artistB" };

        {
            auto transaction{ session.createWriteTransaction() };

            artistA.get().modify()->setSortName("sortNameB");
            artistB.get().modify()->setSortName("sortNameA");
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto allArtistsByName{ Artist::findIds(session, Artist::FindParameters{}.setSortMethod(ArtistSortMethod::Name)) };
            auto allArtistsBySortName{ Artist::findIds(session, Artist::FindParameters{}.setSortMethod(ArtistSortMethod::SortName)) };

            ASSERT_EQ(allArtistsByName.results.size(), 2);
            EXPECT_EQ(allArtistsByName.results.front(), artistA.getId());
            EXPECT_EQ(allArtistsByName.results.back(), artistB.getId());

            ASSERT_EQ(allArtistsBySortName.results.size(), 2);
            EXPECT_EQ(allArtistsBySortName.results.front(), artistB.getId());
            EXPECT_EQ(allArtistsBySortName.results.back(), artistA.getId());
        }
    }

    TEST_F(DatabaseFixture, Artist_nonReleaseTracks)
    {
        ScopedArtist artist{ session, "artist" };
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createReadTransaction() };

            auto tracks{ Track::findIds(session, Track::FindParameters{}.setNonRelease(true).setArtist(artist->getId())) };
            EXPECT_EQ(tracks.results.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            TrackArtistLink::create(session, track1.get(), artist.get(), TrackArtistLinkType::Artist);
            TrackArtistLink::create(session, track2.get(), artist.get(), TrackArtistLinkType::Artist);

            track1.get().modify()->setRelease(release.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto tracks{ Track::findIds(session, Track::FindParameters{}.setArtist(artist.getId()).setNonRelease(true)) };
            ASSERT_EQ(tracks.results.size(), 1);
            EXPECT_EQ(tracks.results.front(), track2.getId());
        }
    }

    TEST_F(DatabaseFixture, Artist_findByRelease)
    {
        ScopedArtist artist{ session, "artist" };
        ScopedTrack track{ session };
        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createReadTransaction() };
            const auto artists{ Artist::findIds(session, Artist::FindParameters{}.setRelease(release.getId())) };
            EXPECT_EQ(artists.results.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto artists{ Artist::findIds(session, Artist::FindParameters{}.setRelease(release.getId())) };
            EXPECT_EQ(artists.results.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setRelease(release.get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto artists{ Artist::findIds(session, Artist::FindParameters{}.setRelease(release.getId())) };
            ASSERT_EQ(artists.results.size(), 1);
            EXPECT_EQ(artists.results.front(), artist.getId());
        }
    }

    TEST_F(DatabaseFixture, Artist_image)
    {
        ScopedArtist release{ session, "MyArtist" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_FALSE(release.get()->getImage());
        }

        ScopedImage image{ session, "/myImage" };

        {
            auto transaction{ session.createWriteTransaction() };
            release.get().modify()->setImage(image.get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto artistImage(release.get()->getImage());
            ASSERT_TRUE(artistImage);
            EXPECT_EQ(artistImage->getId(), image.getId());
        }
    }
} // namespace lms::db::tests
