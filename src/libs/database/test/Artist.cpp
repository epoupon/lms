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

#include "database/objects/ArtistInfo.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Image.hpp"
#include "database/objects/ReleaseArtistLink.hpp"

namespace lms::db::tests
{
    using ScopedArtwork = ScopedEntity<db::Artwork>;
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
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front(), artist.getId());

            artists = Artist::findOrphanIds(session);
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front(), artist.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ Artist::find(session, Artist::FindParameters{}) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front()->getId(), artist.getId());
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
        ScopedMediaLibrary library{ session, "MyLibrary", "/root" };
        ScopedMediaLibrary otherLibrary{ session, "OtherLibrary", "/otherRoot" };

        {
            auto transaction{ session.createWriteTransaction() };
            track2a.get().modify()->setMediaLibrary(library.get());
            track2b.get().modify()->setMediaLibrary(library.get());
            session.create<TrackArtistLink>(track1.get(), artist1.get(), TrackArtistLinkType::Artist);
            session.create<TrackArtistLink>(track2a.get(), artist2.get(), TrackArtistLinkType::Artist);
            session.create<TrackArtistLink>(track2b.get(), artist2.get(), TrackArtistLinkType::Artist);
            session.create<TrackArtistLink>(track3.get(), artist3.get(), TrackArtistLinkType::Artist);
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
            ASSERT_EQ(artists.size(), 0);
        }

        ScopedArtist artist1{ session, "MyArtist1" };
        ScopedArtist artist2{ session, "MyArtist2" };
        ScopedArtist artist3{ session, "MyArtist3" };

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ Artist::findIds(session, Artist::FindParameters{}) };
            ASSERT_EQ(artists.size(), 3);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setRange(Range{ 0, 1 })) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists[0], artist1.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setRange(Range{ 1, 1 })) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists[0], artist2.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setRange(Range{ 2, 1 })) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists[0], artist3.getId());
        }
    }

    TEST_F(DatabaseFixture, Artist_singleTrack)
    {
        ScopedTrack track{ session };
        ScopedArtist artist{ session, "MyArtist" };

        {
            auto transaction{ session.createWriteTransaction() };

            track.get().modify()->setName("MyTrackName");
            session.create<TrackArtistLink>(track.get(), artist.get(), TrackArtistLinkType::Artist);
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Artist::findOrphanIds(session).size(), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ track->getArtists({ TrackArtistLinkType::Artist }) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front()->getId(), artist.getId());

            {
                bool visited{};
                track->visitArtistLinks([&](const db::TrackArtistLink::pointer& artistLink) {
                    visited = true;
                    EXPECT_EQ(artistLink->getArtistId(), artist.getId());
                });
                EXPECT_TRUE(visited);
            }

            {
                bool visited{};
                track->visitArtistLinks(TrackArtistLinkType::Artist, [&](const db::TrackArtistLink::pointer& artistLink) {
                    visited = true;
                    EXPECT_EQ(artistLink->getArtistId(), artist.getId());
                });
                EXPECT_TRUE(visited);
            }

            {
                bool visited{};
                track->visitArtistLinks(TrackArtistLinkType::Conductor, [&](const db::TrackArtistLink::pointer&) {
                    visited = true;
                });
                EXPECT_FALSE(visited);
            }

            ASSERT_EQ(track->getArtistLinks().size(), 1);
            auto artistLink{ track->getArtistLinks().front() };
            EXPECT_EQ(artistLink->getTrack()->getId(), track.getId());
            EXPECT_EQ(artistLink->getArtist()->getId(), artist.getId());

            ASSERT_EQ(track->getArtists({ TrackArtistLinkType::Artist }).size(), 1);
            EXPECT_EQ(track->getArtists({ TrackArtistLinkType::Composer }).size(), 0);
            EXPECT_EQ(track->getArtists({}).size(), 1);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ track->getArtistIds({ TrackArtistLinkType::Artist }) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front(), artist.getId());

            ASSERT_EQ(track->getArtistIds({ TrackArtistLinkType::Artist }).size(), 1);
            EXPECT_EQ(track->getArtistIds({ TrackArtistLinkType::Composer }).size(), 0);
            EXPECT_EQ(track->getArtistIds({}).size(), 1);
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto tracks{ Track::findIds(session, Track::FindParameters{}.setName("MyTrackName").setArtistName("MyArtist")) };
            ASSERT_EQ(tracks.size(), 1);
            EXPECT_EQ(tracks.front(), track.getId());
        }
        {
            auto transaction{ session.createReadTransaction() };
            auto tracks{ Track::findIds(session, Track::FindParameters{}.setName("MyTrackName").setArtistName("MyArtistFoo")) };
            EXPECT_EQ(tracks.size(), 0);
        }
        {
            auto transaction{ session.createReadTransaction() };
            auto tracks{ Track::findIds(session, Track::FindParameters{}.setName("MyTrackNameFoo").setArtistName("MyArtist")) };
            EXPECT_EQ(tracks.size(), 0);
        }
        {
            auto transaction{ session.createReadTransaction() };
            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setTrack(track->getId())) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front(), artist.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Artist::find(session, Artist::FindParameters{}.setReleaseArtistsOnly(true)).size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Artist_singleRelease)
    {
        ScopedRelease release{ session, "MyRelease" };
        ScopedArtist artist{ session, "MyArtist" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Artist::findOrphanIds(session), std::vector<db::ArtistId>{ artist.getId() });
        }

        {
            auto transaction{ session.createWriteTransaction() };
            session.create<ReleaseArtistLink>(release.get(), artist.get(), false);
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Artist::findOrphanIds(session).size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Artist_singleTrack_mediaLibrary)
    {
        ScopedTrack track{ session };
        ScopedArtist artist{ session, "MyArtist" };
        ScopedMediaLibrary library{ session, "MyLibrary", "/root" };
        ScopedMediaLibrary otherLibrary{ session, "OtherLibrary", "/otherRoot" };

        {
            auto transaction{ session.createWriteTransaction() };

            track.get().modify()->setName("MyTrackName");
            session.create<TrackArtistLink>(track.get(), artist.get(), TrackArtistLinkType::Artist);
            track.get().modify()->setMediaLibrary(library.get());
        }
        {
            auto transaction{ session.createReadTransaction() };
            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setTrack(track->getId())) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front(), artist.getId());
        }
        {
            auto transaction{ session.createReadTransaction() };
            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setFilter(FilterExpr::equal(library->getId()))) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front(), artist.getId());
        }
        {
            auto transaction{ session.createReadTransaction() };
            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setFilter(FilterExpr::equal(otherLibrary->getId()))) };
            EXPECT_EQ(artists.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Artist_singleTrack_singleRelease_mediaLibrary)
    {
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedArtist artist{ session, "MyArtist" };
        ScopedRelease release2{ session, "MyRelease" };
        ScopedMediaLibrary library1{ session, "Library1", "/root1" };
        ScopedMediaLibrary library2{ session, "Library2", "/root2" };
        ScopedMediaLibrary otherLibrary{ session, "OtherLibrary", "/otherRoot" };

        {
            auto transaction{ session.createWriteTransaction() };

            track1.get().modify()->setName("Track1");
            session.create<TrackArtistLink>(track1.get(), artist.get(), TrackArtistLinkType::Artist);
            track1.get().modify()->setMediaLibrary(library1.get());

            track2.get().modify()->setName("Track2");
            track2.get().modify()->setRelease(release2.get());
            session.create<ReleaseArtistLink>(release2.get(), artist.get(), false);
            track2.get().modify()->setMediaLibrary(library2.get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto artists{ Artist::findIds(session, Artist::FindParameters{}) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front(), artist.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setFilter(FilterExpr::equal(library1->getId()))) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front(), artist.getId());
        }
        {
            auto transaction{ session.createReadTransaction() };
            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setFilter(FilterExpr::equal(library2->getId()))) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front(), artist.getId());
        }
        {
            auto transaction{ session.createReadTransaction() };
            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setFilter(FilterExpr::equal(otherLibrary->getId()))) };
            EXPECT_EQ(artists.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Artist_singleTrack_singleRelease_single_mediaLibrary)
    {
        ScopedTrack track{ session };
        ScopedArtist artist{ session, "MyArtist" };
        ScopedRelease release{ session, "MyRelease" };
        ScopedMediaLibrary library{ session, "Library1", "/root1" };

        {
            auto transaction{ session.createWriteTransaction() };

            track.get().modify()->setName("Track1");
            track.get().modify()->setRelease(release.get());
            track.get().modify()->setMediaLibrary(library.get());

            session.create<TrackArtistLink>(track.get(), artist.get(), TrackArtistLinkType::Artist);
            session.create<ReleaseArtistLink>(release.get(), artist.get(), false);
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto artists{ Artist::findIds(session, Artist::FindParameters{}) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front(), artist.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setFilter(FilterExpr::equal(library->getId()))) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front(), artist.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            Artist::FindParameters params;
            params.setTrackArtistLinkType(TrackArtistLinkType::Artist);
            params.setFilter(FilterExpr::equal(library.getId()));

            auto artists{ Artist::findIds(session, params) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front(), artist.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            Artist::FindParameters params;
            params.setTrackArtistLinkType(TrackArtistLinkType::Performer);
            params.setFilter(FilterExpr::equal(library.getId()));

            auto artists{ Artist::findIds(session, params) };
            EXPECT_EQ(artists.size(), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Artist::FindParameters params;
            params.setReleaseArtistsOnly(true);
            params.setFilter(FilterExpr::equal(library.getId()));

            auto artists{ Artist::findIds(session, params) };
            EXPECT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front(), artist.getId());
        }
    }

    TEST_F(DatabaseFixture, Artist_singleTracktMultiRoles)
    {
        ScopedTrack track{ session };
        ScopedArtist artist{ session, "MyArtist" };
        {
            auto transaction{ session.createWriteTransaction() };

            session.create<TrackArtistLink>(track.get(), artist.get(), TrackArtistLinkType::Artist);
            session.create<TrackArtistLink>(track.get(), artist.get(), TrackArtistLinkType::Producer);
            session.create<TrackArtistLink>(track.get(), artist.get(), TrackArtistLinkType::Writer);
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Artist::findOrphanIds(session, Range{}).size(), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Artist::findIds(session, Artist::FindParameters{}).size(), 1);
            EXPECT_EQ(Artist::findIds(session, Artist::FindParameters{}.setTrackArtistLinkType(TrackArtistLinkType::Artist)).size(), 1);
            EXPECT_EQ(Artist::findIds(session, Artist::FindParameters{}.setTrackArtistLinkType(TrackArtistLinkType::Producer)).size(), 1);
            EXPECT_EQ(Artist::findIds(session, Artist::FindParameters{}.setTrackArtistLinkType(TrackArtistLinkType::Writer)).size(), 1);
            EXPECT_EQ(Artist::findIds(session, Artist::FindParameters{}.setTrackArtistLinkType(TrackArtistLinkType::Composer)).size(), 0);

            EXPECT_EQ(Artist::findIds(session, Artist::FindParameters{}.setReleaseArtistsOnly(true)).size(), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ track->getArtists({ TrackArtistLinkType::Artist }) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front()->getId(), artist.getId());

            artists = track->getArtists({ TrackArtistLinkType::Producer });
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front()->getId(), artist.getId());

            EXPECT_EQ(track->getArtistLinks().size(), 3);

            auto tracks{ Track::findIds(session, Track::FindParameters{}.setArtist(artist.getId())) };
            EXPECT_EQ(tracks.size(), 1);

            tracks = Track::findIds(session, Track::FindParameters{}.setArtist(artist.getId(), { TrackArtistLinkType::Producer }));
            EXPECT_EQ(tracks.size(), 1);
            tracks = Track::findIds(session, Track::FindParameters{}.setArtist(artist.getId(), { TrackArtistLinkType::Artist }));
            EXPECT_EQ(tracks.size(), 1);
            tracks = Track::findIds(session, Track::FindParameters{}.setArtist(artist.getId(), { TrackArtistLinkType::Writer }));
            EXPECT_EQ(tracks.size(), 1);

            tracks = Track::findIds(session, Track::FindParameters{}.setArtist(artist.getId(), { TrackArtistLinkType::Composer }));
            EXPECT_EQ(tracks.size(), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };
            core::EnumSet<TrackArtistLinkType> types{ TrackArtistLink::findUsedTypes(session, artist.getId()) };
            EXPECT_TRUE(types.contains(TrackArtistLinkType::Producer));
            EXPECT_TRUE(types.contains(TrackArtistLinkType::Artist));
            EXPECT_TRUE(types.contains(TrackArtistLinkType::Writer));
            EXPECT_FALSE(types.contains(TrackArtistLinkType::Composer));
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::vector<TrackArtistLink::pointer> visitedLinks;
            track->visitArtistLinks([&](const TrackArtistLink::pointer& link) {
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
            EXPECT_TRUE(containsType(TrackArtistLinkType::Producer));
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

            session.create<TrackArtistLink>(track.get(), artist1.get(), TrackArtistLinkType::Artist);
            session.create<TrackArtistLink>(track.get(), artist2.get(), TrackArtistLinkType::Artist);
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Artist::findOrphanIds(session).size(), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ track->getArtists({ TrackArtistLinkType::Artist }) };
            ASSERT_EQ(artists.size(), 2);
            EXPECT_TRUE((artists[0]->getId() == artist1.getId() && artists[1]->getId() == artist2.getId())
                        || (artists[0]->getId() == artist2.getId() && artists[1]->getId() == artist1.getId()));

            EXPECT_EQ(track->getArtists({}).size(), 2);
            EXPECT_EQ(track->getArtists({ TrackArtistLinkType::Artist }).size(), 2);
            EXPECT_EQ(track->getArtists({ TrackArtistLinkType::Arranger }).size(), 0);
            EXPECT_EQ(Artist::findIds(session, Artist::FindParameters{}).size(), 2);
            EXPECT_EQ(Artist::findIds(session, Artist::FindParameters{}.setSortMethod(ArtistSortMethod::Random)).size(), 2);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto tracks{ Track::findIds(session, Track::FindParameters{}.setArtist(artist1->getId())) };
            ASSERT_EQ(tracks.size(), 1);
            EXPECT_EQ(tracks.front(), track->getId());

            tracks = Track::findIds(session, Track::FindParameters{}.setArtist(artist2->getId()));
            ASSERT_EQ(tracks.size(), 1);
            EXPECT_EQ(tracks.front(), track->getId());

            tracks = Track::findIds(session, Track::FindParameters{}.setArtist(artist1->getId(), { TrackArtistLinkType::Arranger }));
            EXPECT_EQ(tracks.size(), 0);

            tracks = Track::findIds(session, Track::FindParameters{}.setArtist(artist1->getId(), { TrackArtistLinkType::Artist }));
            EXPECT_EQ(tracks.size(), 1);

            tracks = Track::findIds(session, Track::FindParameters{}.setArtist(artist2->getId(), { TrackArtistLinkType::Arranger }));
            EXPECT_EQ(tracks.size(), 0);

            tracks = Track::findIds(session, Track::FindParameters{}.setArtist(artist2->getId(), { TrackArtistLinkType::Artist }));
            EXPECT_EQ(tracks.size(), 1);
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::vector<TrackArtistLink::pointer> visitedLinks;
            track->visitArtistLinks([&](const TrackArtistLink::pointer& link) {
                visitedLinks.push_back(link);
            });
            ASSERT_EQ(visitedLinks.size(), 2);
            EXPECT_EQ(visitedLinks[0]->getArtist()->getId(), artist1.getId());
            EXPECT_EQ(visitedLinks[1]->getArtist()->getId(), artist2.getId());
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

    TEST_F(DatabaseFixture, Artist_findByReleaseArtistLinks)
    {
        ScopedRelease release{ session, "release" };
        ScopedArtist artist{ session, "artist" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Artist::find(session, Artist::FindParameters{}.setReleaseArtistsOnly(true)).size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            session.create<ReleaseArtistLink>(release.get(), artist.get(), false);
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Artist::find(session, Artist::FindParameters{}.setReleaseArtistsOnly(true)).size(), 1);
        }
    }

    TEST_F(DatabaseFixture, Artist_findByCodec)
    {
        ScopedArtist artist1{ session, "A" };
        ScopedArtist artist2{ session, "B" };
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            session.create<TrackArtistLink>(track1.get(), artist1.get(), TrackArtistLinkType::Artist);
            session.create<TrackArtistLink>(track2.get(), artist2.get(), TrackArtistLinkType::Artist);

            track1.get().modify()->setCodec(core::media::Codec::MP3);
            track2.get().modify()->setCodec(core::media::Codec::FLAC);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Artist::FindParameters params;
            params.setFilter(FilterExpr::equal(core::media::Codec::FLAC));

            const auto artists{ Artist::find(session, params) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front()->getId(), artist2.getId());
        }
    }

    TEST_F(DatabaseFixture, Artist_findByName)
    {
        ScopedArtist artist{ session, "AAA" };
        ScopedTrack track{ session }; // filters does not work on orphans

        {
            auto transaction{ session.createWriteTransaction() };
            artist.get().modify()->setSortName("ZZZ");
            session.create<TrackArtistLink>(track.get(), artist.get(), TrackArtistLinkType::Artist);
        }

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(Artist::findIds(session, Artist::FindParameters{}.setKeywords({ "N" })).size(), 0);

            const auto artistsByAAA{ Artist::findIds(session, Artist::FindParameters{}.setKeywords({ "A" })) };
            ASSERT_EQ(artistsByAAA.size(), 1);
            EXPECT_EQ(artistsByAAA.front(), artist.getId());

            const auto artistsByZZZ{ Artist::Artist::findIds(session, Artist::FindParameters{}.setKeywords({ "Z" })) };
            ASSERT_EQ(artistsByZZZ.size(), 1);
            EXPECT_EQ(artistsByZZZ.front(), artist.getId());

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
                EXPECT_EQ(artists.size(), 6);
            }

            {
                const auto artists{ Artist::findIds(session, Artist::FindParameters{}.setKeywords({ "MyArtist%" }).setSortMethod(ArtistSortMethod::Name)) };
                ASSERT_EQ(artists.size(), 2);
                EXPECT_EQ(artists[0], artist1.getId());
                EXPECT_EQ(artists[1], artist4.getId());
            }

            {
                const auto artists{ Artist::findIds(session, Artist::FindParameters{}.setKeywords({ "%MyArtist" }).setSortMethod(ArtistSortMethod::Name)) };
                ASSERT_EQ(artists.size(), 2);
                EXPECT_EQ(artists[0], artist2.getId());
                EXPECT_EQ(artists[1], artist5.getId());
            }

            {
                const auto artists{ Artist::findIds(session, Artist::FindParameters{}.setKeywords({ "_MyArtist" }).setSortMethod(ArtistSortMethod::Name)) };
                ASSERT_EQ(artists.size(), 1);
                EXPECT_EQ(artists[0], artist3.getId());
            }
        }
    }

    TEST_F(DatabaseFixture, Artist_findByNameEscapedBackslash)
    {
        ScopedArtist artist1{ session, R"(AC\DC)" };
        ScopedArtist artist2{ session, "ACDC" };
        ScopedArtist artist3{ session, R"(MyArtist\)" };

        {
            auto transaction{ session.createReadTransaction() };

            {
                const auto artists{ Artist::findIds(session, Artist::FindParameters{}.setKeywords({ R"(AC\DC)" })) };
                ASSERT_EQ(artists.size(), 1);
                EXPECT_EQ(artists[0], artist1.getId());
            }
            {
                const auto artists{ Artist::findIds(session, Artist::FindParameters{}.setKeywords({ R"(MyArtist\)" })) };
                ASSERT_EQ(artists.size(), 1);
                EXPECT_EQ(artists[0], artist3.getId());
            }
        }
    }

    TEST_F(DatabaseFixture, Artist_findNextIdRange)
    {
        {
            auto transaction{ session.createReadTransaction() };

            auto range{ Artist::findNextIdRange(session, ArtistId{}, 0) };
            EXPECT_FALSE(range.isValid());
            EXPECT_EQ(range.first, ArtistId{});
            EXPECT_EQ(range.last, ArtistId{});

            range = Artist::findNextIdRange(session, ArtistId{}, 100);
            EXPECT_FALSE(range.isValid());
            EXPECT_EQ(range.first, ArtistId{});
            EXPECT_EQ(range.last, ArtistId{});
        }

        ScopedArtist artist1{ session, "Artist1" };
        {
            auto transaction{ session.createReadTransaction() };

            auto range{ Artist::findNextIdRange(session, ArtistId{}, 0) };
            EXPECT_FALSE(range.isValid());
            EXPECT_EQ(range.first, ArtistId{});
            EXPECT_EQ(range.last, ArtistId{});

            range = Artist::findNextIdRange(session, ArtistId{}, 1);
            EXPECT_TRUE(range.isValid());
            EXPECT_EQ(range.first, artist1.getId());
            EXPECT_EQ(range.last, artist1.getId());

            range = Artist::findNextIdRange(session, range.last, 1);
            EXPECT_FALSE(range.isValid());
            EXPECT_EQ(range.first, ArtistId{});
            EXPECT_EQ(range.last, ArtistId{});

            range = Artist::findNextIdRange(session, ArtistId{}, 100);
            EXPECT_TRUE(range.isValid());
            EXPECT_EQ(range.first, artist1.getId());
            EXPECT_EQ(range.last, artist1.getId());
        }

        ScopedArtist artist2{ session, "Artist2" };
        ScopedArtist artist3{ session, "Artist3" };

        {
            auto transaction{ session.createReadTransaction() };

            auto range{ Artist::findNextIdRange(session, ArtistId{}, 2) };
            EXPECT_TRUE(range.isValid());
            EXPECT_EQ(range.first, artist1.getId());
            EXPECT_EQ(range.last, artist2.getId());

            range = Artist::findNextIdRange(session, artist2.getId(), 2);
            EXPECT_TRUE(range.isValid());
            EXPECT_EQ(range.first, artist3.getId());
            EXPECT_EQ(range.last, artist3.getId());
        }
    }

    TEST_F(DatabaseFixture, Artist_findByRange)
    {
        ScopedArtist artist1{ session, "Artist1" };
        ScopedArtist artist2{ session, "Artist2" };
        ScopedArtist artist3{ session, "Artist3" };

        {
            auto transaction{ session.createReadTransaction() };

            std::size_t count{};
            Artist::find(session, IdRange<ArtistId>{ .first = artist1.getId(), .last = artist1.getId() }, [&](const db::Artist::pointer& artist) {
                count++;
                EXPECT_EQ(artist->getId(), artist1.getId());
            });
            EXPECT_EQ(count, 1);
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::size_t count{};
            Artist::find(session, IdRange<ArtistId>{ .first = artist1.getId(), .last = artist3.getId() }, [&](const db::Artist::pointer&) {
                count++;
            });
            EXPECT_EQ(count, 3);
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

            ASSERT_EQ(allArtistsByName.size(), 2);
            EXPECT_EQ(allArtistsByName.front(), artistA.getId());
            EXPECT_EQ(allArtistsByName.back(), artistB.getId());

            ASSERT_EQ(allArtistsBySortName.size(), 2);
            EXPECT_EQ(allArtistsBySortName.front(), artistB.getId());
            EXPECT_EQ(allArtistsBySortName.back(), artistA.getId());
        }
    }

    TEST_F(DatabaseFixture, Artist_sortMethod_emptySortNameFallsBackOnName)
    {
        ScopedArtist artistBravo{ session, "Bravo" };     // no sort name -> falls back on "Bravo"
        ScopedArtist artistAlpha{ session, "Alpha" };     // explicit sort name -> "Zulu"
        ScopedArtist artistCharlie{ session, "Charlie" }; // no sort name -> falls back on "Charlie"

        {
            auto transaction{ session.createWriteTransaction() };

            artistAlpha.get().modify()->setSortName("Zulu");
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto allArtistsBySortName{ Artist::findIds(session, Artist::FindParameters{}.setSortMethod(ArtistSortMethod::SortName)) };

            ASSERT_EQ(allArtistsBySortName.size(), 3);
            EXPECT_EQ(allArtistsBySortName[0], artistBravo.getId());
            EXPECT_EQ(allArtistsBySortName[1], artistCharlie.getId());
            EXPECT_EQ(allArtistsBySortName[2], artistAlpha.getId());
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
            EXPECT_EQ(tracks.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            session.create<TrackArtistLink>(track1.get(), artist.get(), TrackArtistLinkType::Artist);
            session.create<TrackArtistLink>(track2.get(), artist.get(), TrackArtistLinkType::Artist);

            track1.get().modify()->setRelease(release.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto tracks{ Track::findIds(session, Track::FindParameters{}.setArtist(artist.getId()).setNonRelease(true)) };
            ASSERT_EQ(tracks.size(), 1);
            EXPECT_EQ(tracks.front(), track2.getId());
        }
    }

    TEST_F(DatabaseFixture, Artist_artwork)
    {
        ScopedImage image{ session, "/image1.jpg" };
        ScopedArtwork artwork{ session, image.lockAndGet() };

        ScopedArtist artist{ session, "MyArtist" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_FALSE(artist.get()->getPreferredArtwork());
        }

        {
            auto transaction{ session.createWriteTransaction() };
            artist.get().modify()->setPreferredArtwork(artwork.get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto artistArtwork(artist.get()->getPreferredArtwork());
            ASSERT_TRUE(artistArtwork);
            EXPECT_EQ(artistArtwork->getId(), artwork.getId());
        }
    }

    TEST_F(DatabaseFixture, Artist_sortDateAdded)
    {
        ScopedArtist artistA{ session, "artistA" };
        ScopedArtist artistB{ session, "artistB" };
        ScopedArtist artistC{ session, "artistC" };
        ScopedArtist artistD{ session, "artistD" };

        ScopedTrack trackA1{ session };
        ScopedTrack trackB1{ session };
        ScopedTrack trackC1{ session };
        ScopedTrack trackD1{ session };

        ScopedTrack trackA2{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            trackA1.get().modify()->setAddedTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 2 } });
            trackB1.get().modify()->setAddedTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 1 } });
            trackD1.get().modify()->setAddedTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 2 }, Wt::WTime{ 15, 36, 24 } });
            trackD1.get().modify()->setAddedTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 3 } });
            trackA2.get().modify()->setAddedTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 4 } });

            session.create<TrackArtistLink>(trackA1.get(), artistA.get(), TrackArtistLinkType::Artist);
            session.create<TrackArtistLink>(trackA2.get(), artistA.get(), TrackArtistLinkType::Producer);
            session.create<TrackArtistLink>(trackB1.get(), artistB.get(), TrackArtistLinkType::Artist);
            session.create<TrackArtistLink>(trackC1.get(), artistC.get(), TrackArtistLinkType::Artist);
            session.create<TrackArtistLink>(trackD1.get(), artistD.get(), TrackArtistLinkType::Artist);
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto artists{ Artist::findIds(session, Artist::FindParameters{}.setSortMethod(ArtistSortMethod::AddedDesc)) };
            ASSERT_EQ(artists.size(), 4);
            EXPECT_EQ(artists[0], artistD.getId());
            EXPECT_EQ(artists[1], artistA.getId());
            EXPECT_EQ(artists[2], artistB.getId());
            EXPECT_EQ(artists[3], artistC.getId());
        }
    }

    TEST_F(DatabaseFixture, Artist_sortDateAdded_tieBreakOnEffectiveSortName)
    {
        ScopedArtist artistZulu{ session, "Zulu" };   // no sort name -> falls back on "Zulu"
        ScopedArtist artistAlpha{ session, "Alpha" }; // explicit sort name -> "Aardvark"

        ScopedTrack trackZulu{ session };
        ScopedTrack trackAlpha{ session };

        {
            auto transaction{ session.createWriteTransaction() };

            artistAlpha.get().modify()->setSortName("Aardvark");

            const Wt::WDateTime addedTime{ Wt::WDate{ 2021, 1, 2 } };
            trackZulu.get().modify()->setAddedTime(addedTime);
            trackAlpha.get().modify()->setAddedTime(addedTime);

            session.create<TrackArtistLink>(trackZulu.get(), artistZulu.get(), TrackArtistLinkType::Artist);
            session.create<TrackArtistLink>(trackAlpha.get(), artistAlpha.get(), TrackArtistLinkType::Artist);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto artists{ Artist::findIds(session, Artist::FindParameters{}.setSortMethod(ArtistSortMethod::AddedDesc)) };
            ASSERT_EQ(artists.size(), 2);
            EXPECT_EQ(artists[0], artistAlpha.getId());
            EXPECT_EQ(artists[1], artistZulu.getId());
        }
    }

    TEST_F(DatabaseFixture, Artist_sortLastWritten)
    {
        ScopedArtist artistA{ session, "artistA" };
        ScopedArtist artistB{ session, "artistB" };
        ScopedArtist artistC{ session, "artistC" };
        ScopedArtist artistD{ session, "artistD" };

        ScopedTrack trackA1{ session };
        ScopedTrack trackB1{ session };
        ScopedTrack trackC1{ session };
        ScopedTrack trackD1{ session };

        ScopedTrack trackA2{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            trackA1.get().modify()->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 2 } });
            trackB1.get().modify()->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 1 } });
            trackD1.get().modify()->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 2 }, Wt::WTime{ 15, 36, 24 } });
            trackD1.get().modify()->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 3 } });
            trackA2.get().modify()->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 4 } });

            session.create<TrackArtistLink>(trackA1.get(), artistA.get(), TrackArtistLinkType::Artist);
            session.create<TrackArtistLink>(trackA2.get(), artistA.get(), TrackArtistLinkType::Producer);
            session.create<TrackArtistLink>(trackB1.get(), artistB.get(), TrackArtistLinkType::Artist);
            session.create<TrackArtistLink>(trackC1.get(), artistC.get(), TrackArtistLinkType::Artist);
            session.create<TrackArtistLink>(trackD1.get(), artistD.get(), TrackArtistLinkType::Artist);
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto artists{ Artist::findIds(session, Artist::FindParameters{}.setSortMethod(ArtistSortMethod::LastWrittenDesc)) };
            ASSERT_EQ(artists.size(), 4);
            EXPECT_EQ(artists[0], artistA.getId());
            EXPECT_EQ(artists[1], artistD.getId());
            EXPECT_EQ(artists[2], artistB.getId());
            EXPECT_EQ(artists[3], artistC.getId());
        }
    }

    TEST_F(DatabaseFixture, Artist_sortLastWritten_tieBreakOnEffectiveSortName)
    {
        ScopedArtist artistZulu{ session, "Zulu" };   // no sort name -> falls back on "Zulu"
        ScopedArtist artistAlpha{ session, "Alpha" }; // explicit sort name -> "Aardvark"

        ScopedTrack trackZulu{ session };
        ScopedTrack trackAlpha{ session };

        {
            auto transaction{ session.createWriteTransaction() };

            artistAlpha.get().modify()->setSortName("Aardvark");

            const Wt::WDateTime lastWriteTime{ Wt::WDate{ 2021, 1, 2 } };
            trackZulu.get().modify()->setLastWriteTime(lastWriteTime);
            trackAlpha.get().modify()->setLastWriteTime(lastWriteTime);

            session.create<TrackArtistLink>(trackZulu.get(), artistZulu.get(), TrackArtistLinkType::Artist);
            session.create<TrackArtistLink>(trackAlpha.get(), artistAlpha.get(), TrackArtistLinkType::Artist);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto artists{ Artist::findIds(session, Artist::FindParameters{}.setSortMethod(ArtistSortMethod::LastWrittenDesc)) };
            ASSERT_EQ(artists.size(), 2);
            EXPECT_EQ(artists[0], artistAlpha.getId());
            EXPECT_EQ(artists[1], artistZulu.getId());
        }
    }

    TEST_F(DatabaseFixture, Artist_sortLastWritten_releaseArtist)
    {
        ScopedArtist trackArtist{ session, "trackArtist" };
        ScopedArtist releaseArtist{ session, "releaseArtist" };
        ScopedArtist trackAndReleaseArtist{ session, "trackAndReleaseArtist" };
        ScopedArtist artistWithoutTrack{ session, "artistWithoutTrack" };

        ScopedRelease release1{ session, "release1" };
        ScopedRelease release2{ session, "release2" };

        ScopedTrack trackOfTrackArtist{ session };
        ScopedTrack trackOfRelease1{ session };
        ScopedTrack oldTrackOfBothArtist{ session };
        ScopedTrack trackOfRelease2{ session };

        {
            auto transaction{ session.createWriteTransaction() };

            trackOfTrackArtist.get().modify()->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 2 } });
            trackOfRelease1.get().modify()->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 5 } });
            oldTrackOfBothArtist.get().modify()->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 1 } });
            trackOfRelease2.get().modify()->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 6 } });

            trackOfRelease1.get().modify()->setRelease(release1.get());
            trackOfRelease2.get().modify()->setRelease(release2.get());

            session.create<TrackArtistLink>(trackOfTrackArtist.get(), trackArtist.get(), TrackArtistLinkType::Artist);
            session.create<ReleaseArtistLink>(release1.get(), releaseArtist.get(), false);
            session.create<TrackArtistLink>(oldTrackOfBothArtist.get(), trackAndReleaseArtist.get(), TrackArtistLinkType::Artist);
            session.create<ReleaseArtistLink>(release2.get(), trackAndReleaseArtist.get(), false);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto artists{ Artist::findIds(session, Artist::FindParameters{}.setSortMethod(ArtistSortMethod::LastWrittenDesc)) };
            ASSERT_EQ(artists.size(), 3);
            EXPECT_EQ(artists[0], trackAndReleaseArtist.getId());
            EXPECT_EQ(artists[1], releaseArtist.getId());
            EXPECT_EQ(artists[2], trackArtist.getId());
        }
    }

    TEST_F(DatabaseFixture, Artist_sortDateAdded_releaseArtist)
    {
        ScopedArtist trackArtist{ session, "trackArtist" };
        ScopedArtist releaseArtist{ session, "releaseArtist" };
        ScopedArtist trackAndReleaseArtist{ session, "trackAndReleaseArtist" };
        ScopedArtist artistWithoutTrack{ session, "artistWithoutTrack" };

        ScopedRelease release1{ session, "release1" };
        ScopedRelease release2{ session, "release2" };

        ScopedTrack trackOfTrackArtist{ session };
        ScopedTrack trackOfRelease1{ session };
        ScopedTrack oldTrackOfBothArtist{ session };
        ScopedTrack trackOfRelease2{ session };

        {
            auto transaction{ session.createWriteTransaction() };

            trackOfTrackArtist.get().modify()->setAddedTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 2 } });
            trackOfRelease1.get().modify()->setAddedTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 5 } });
            oldTrackOfBothArtist.get().modify()->setAddedTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 1 } });
            trackOfRelease2.get().modify()->setAddedTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 6 } });

            trackOfRelease1.get().modify()->setRelease(release1.get());
            trackOfRelease2.get().modify()->setRelease(release2.get());

            session.create<TrackArtistLink>(trackOfTrackArtist.get(), trackArtist.get(), TrackArtistLinkType::Artist);
            session.create<ReleaseArtistLink>(release1.get(), releaseArtist.get(), false);
            session.create<TrackArtistLink>(oldTrackOfBothArtist.get(), trackAndReleaseArtist.get(), TrackArtistLinkType::Artist);
            session.create<ReleaseArtistLink>(release2.get(), trackAndReleaseArtist.get(), false);
        }

        {
            auto transaction{ session.createReadTransaction() };

            // sorted on the oldest track added, whatever the way the artist is linked to it
            const auto artists{ Artist::findIds(session, Artist::FindParameters{}.setSortMethod(ArtistSortMethod::AddedDesc)) };
            ASSERT_EQ(artists.size(), 3);
            EXPECT_EQ(artists[0], releaseArtist.getId());
            EXPECT_EQ(artists[1], trackArtist.getId());
            EXPECT_EQ(artists[2], trackAndReleaseArtist.getId());
        }
    }

    TEST_F(DatabaseFixture, Artist_sortByTrackDate_sameArtistsWithAndWithoutFilter)
    {
        ScopedMediaLibrary library{ session, "MyLibrary", "/root" };
        ScopedArtist trackArtist{ session, "trackArtist" };
        ScopedArtist releaseArtist{ session, "releaseArtist" };
        ScopedRelease release{ session, "release" };

        ScopedTrack trackOfTrackArtist{ session };
        ScopedTrack trackOfRelease{ session };

        {
            auto transaction{ session.createWriteTransaction() };

            trackOfTrackArtist.get().modify()->setMediaLibrary(library.get());
            trackOfTrackArtist.get().modify()->setAddedTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 2 } });
            trackOfTrackArtist.get().modify()->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 2 } });
            trackOfRelease.get().modify()->setMediaLibrary(library.get());
            trackOfRelease.get().modify()->setAddedTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 5 } });
            trackOfRelease.get().modify()->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 5 } });
            trackOfRelease.get().modify()->setRelease(release.get());

            session.create<TrackArtistLink>(trackOfTrackArtist.get(), trackArtist.get(), TrackArtistLinkType::Artist);
            session.create<ReleaseArtistLink>(release.get(), releaseArtist.get(), false);
        }

        for (const ArtistSortMethod sortMethod : { ArtistSortMethod::AddedDesc, ArtistSortMethod::LastWrittenDesc })
        {
            auto transaction{ session.createReadTransaction() };

            const auto unfiltered{ Artist::findIds(session, Artist::FindParameters{}.setSortMethod(sortMethod)) };
            const auto filtered{ Artist::findIds(session, Artist::FindParameters{}.setSortMethod(sortMethod).setFilter(FilterExpr::equal(library.getId()))) };

            ASSERT_EQ(unfiltered.size(), 2);
            EXPECT_EQ(unfiltered[0], releaseArtist.getId());
            EXPECT_EQ(unfiltered[1], trackArtist.getId());
            EXPECT_EQ(unfiltered, filtered);
        }
    }

    TEST_F(DatabaseFixture, Artist_updateArtwork)
    {
        ScopedArtist artist{ session, "MyArtist" };
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(artist->getPreferredArtwork(), Artwork::pointer{});
        }

        ScopedImage image{ session, "/image1.jpg" };
        ScopedArtwork artwork{ session, image.lockAndGet() };

        {
            auto transaction{ session.createWriteTransaction() };
            Artist::updatePreferredArtwork(session, artist->getId(), artwork.getId());
        }
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(artist->getPreferredArtwork()->getId(), artwork.getId());
        }

        {
            auto transaction{ session.createWriteTransaction() };
            Artist::updatePreferredArtwork(session, artist.getId(), ArtworkId{});
        }
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(artist->getPreferredArtwork(), Artwork::pointer{});
        }
    }

    namespace
    {
        const std::optional<core::UUID> artistMBID{ core::UUID::fromString("38811c52-85e3-4e2e-3319-ab7d9f2cfa5b") };

        struct NameUpdate
        {
            ArtistId artist;
            std::string name;
            std::string sortName;
        };

        std::vector<NameUpdate> findNameUpdates(Session& session)
        {
            auto transaction{ session.createReadTransaction() };

            std::vector<NameUpdate> updates;
            Artist::findNameUpdates(session, [&](const Artist::pointer& artist, std::string_view name, std::string_view sortName) {
                updates.push_back(NameUpdate{ .artist = artist->getId(), .name = std::string{ name }, .sortName = std::string{ sortName } });
            });
            return updates;
        }

        void createArtistInfo(Session& session, const Artist::pointer& artist, bool mbidMatched, std::string_view name, std::string_view sortName)
        {
            ArtistInfo::pointer artistInfo{ session.create<ArtistInfo>() };
            artistInfo.modify()->setArtist(artist);
            artistInfo.modify()->setMBIDMatched(mbidMatched);
            artistInfo.modify()->setName(name);
            artistInfo.modify()->setSortName(sortName);
        }

        void createReleaseArtistLink(Session& session, const Release::pointer& release, const Artist::pointer& artist, bool mbidMatched, std::string_view name, std::string_view sortName)
        {
            ReleaseArtistLink::pointer link{ session.create<ReleaseArtistLink>(release, artist, mbidMatched) };
            link.modify()->setArtistName(name);
            link.modify()->setArtistSortName(sortName);
        }

        void createTrackArtistLink(Session& session, const Track::pointer& track, const Artist::pointer& artist, bool mbidMatched, std::string_view name, std::string_view sortName)
        {
            TrackArtistLink::pointer link{ session.create<TrackArtistLink>(track, artist, TrackArtistLinkType::Artist, mbidMatched) };
            link.modify()->setArtistName(name);
            link.modify()->setArtistSortName(sortName);
        }
    } // namespace

    TEST_F(DatabaseFixture, Artist_findNameUpdates_upToDate)
    {
        ScopedArtist artist{ session, "Artist", artistMBID };
        ScopedTrack track{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            artist.get().modify()->setSortName("Artist, Sort");
            createTrackArtistLink(session, track.get(), artist.get(), true, "Artist", "Artist, Sort");
        }

        EXPECT_TRUE(findNameUpdates(session).empty());
    }

    TEST_F(DatabaseFixture, Artist_findNameUpdates_noSource)
    {
        ScopedArtist artistWithSortName{ session, "Artist1" };
        ScopedArtist artistWithoutSortName{ session, "Artist2" };

        {
            auto transaction{ session.createWriteTransaction() };
            artistWithSortName.get().modify()->setSortName("Artist1, Stale");
        }

        const std::vector<NameUpdate> updates{ findNameUpdates(session) };
        ASSERT_EQ(updates.size(), 1);
        EXPECT_EQ(updates[0].artist, artistWithSortName.getId());
        EXPECT_EQ(updates[0].name, "Artist1");
        EXPECT_EQ(updates[0].sortName, "");
    }

    TEST_F(DatabaseFixture, Artist_findNameUpdates_artistInfoFirst)
    {
        ScopedArtist artist{ session, "Artist", artistMBID };
        ScopedRelease release{ session, "Release" };

        {
            auto transaction{ session.createWriteTransaction() };
            createArtistInfo(session, artist.get(), true, "Info Artist", "Info, Sort");
            createReleaseArtistLink(session, release.get(), artist.get(), true, "Artist", "Release, Sort");
        }

        const std::vector<NameUpdate> updates{ findNameUpdates(session) };
        ASSERT_EQ(updates.size(), 1);
        EXPECT_EQ(updates[0].name, "Info Artist");
        EXPECT_EQ(updates[0].sortName, "Info, Sort");
    }

    TEST_F(DatabaseFixture, Artist_findNameUpdates_artistInfoWithoutSortName)
    {
        ScopedArtist artist{ session, "Artist", artistMBID };
        ScopedRelease release{ session, "Release" };

        {
            auto transaction{ session.createWriteTransaction() };
            createArtistInfo(session, artist.get(), true, "Info Artist", "");
            createReleaseArtistLink(session, release.get(), artist.get(), true, "Artist", "Release, Sort");
        }

        const std::vector<NameUpdate> updates{ findNameUpdates(session) };
        ASSERT_EQ(updates.size(), 1);
        EXPECT_EQ(updates[0].name, "Info Artist");
        EXPECT_EQ(updates[0].sortName, "Release, Sort");
    }

    TEST_F(DatabaseFixture, Artist_findNameUpdates_mbidMatchedArtistInfoFirst)
    {
        ScopedArtist artist{ session, "Artist", artistMBID };

        {
            auto transaction{ session.createWriteTransaction() };
            createArtistInfo(session, artist.get(), true, "Artist", "Matched, Sort");
            createArtistInfo(session, artist.get(), false, "Artist", "Other, Sort");
        }

        const std::vector<NameUpdate> updates{ findNameUpdates(session) };
        ASSERT_EQ(updates.size(), 1);
        EXPECT_EQ(updates[0].sortName, "Matched, Sort");
    }

    TEST_F(DatabaseFixture, Artist_findNameUpdates_mbidMatchedReleaseBeforeMBIDMatchedTrack)
    {
        ScopedArtist artist{ session, "Artist", artistMBID };
        ScopedRelease release{ session, "Release" };
        ScopedTrack track{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setDate(core::PartialDateTime{ 2020 }); // more recent, but a lower source
            createReleaseArtistLink(session, release.get(), artist.get(), true, "Artist", "Release, Sort");
            createTrackArtistLink(session, track.get(), artist.get(), true, "Artist", "Track, Sort");
        }

        const std::vector<NameUpdate> updates{ findNameUpdates(session) };
        ASSERT_EQ(updates.size(), 1);
        EXPECT_EQ(updates[0].sortName, "Release, Sort");
    }

    TEST_F(DatabaseFixture, Artist_findNameUpdates_mbidMatchedTrackBeforeOtherRelease)
    {
        ScopedArtist artist{ session, "Artist", artistMBID };
        ScopedRelease release{ session, "Release" };
        ScopedTrack track{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            createReleaseArtistLink(session, release.get(), artist.get(), false, "Artist", "Release, Sort");
            createTrackArtistLink(session, track.get(), artist.get(), true, "Artist", "Track, Sort");
        }

        const std::vector<NameUpdate> updates{ findNameUpdates(session) };
        ASSERT_EQ(updates.size(), 1);
        EXPECT_EQ(updates[0].sortName, "Track, Sort");
    }

    TEST_F(DatabaseFixture, Artist_findNameUpdates_otherReleaseBeforeOtherTrack)
    {
        ScopedArtist artist{ session, "Artist" };
        ScopedRelease release{ session, "Release" };
        ScopedTrack track{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            createReleaseArtistLink(session, release.get(), artist.get(), false, "Artist", "Release, Sort");
            createTrackArtistLink(session, track.get(), artist.get(), false, "Artist", "Track, Sort");
        }

        const std::vector<NameUpdate> updates{ findNameUpdates(session) };
        ASSERT_EQ(updates.size(), 1);
        EXPECT_EQ(updates[0].sortName, "Release, Sort");
    }

    TEST_F(DatabaseFixture, Artist_findNameUpdates_mostRecentTrackFirst)
    {
        ScopedArtist artist{ session, "Artist", artistMBID };
        ScopedTrack recentTrack{ session };
        ScopedTrack oldTrack{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            recentTrack.get().modify()->setDate(core::PartialDateTime{ 2010 });
            oldTrack.get().modify()->setDate(core::PartialDateTime{ 2000 });
            createTrackArtistLink(session, recentTrack.get(), artist.get(), true, "Recent Artist", "Recent, Sort");
            createTrackArtistLink(session, oldTrack.get(), artist.get(), true, "Old Artist", "Old, Sort");
        }

        const std::vector<NameUpdate> updates{ findNameUpdates(session) };
        ASSERT_EQ(updates.size(), 1);
        EXPECT_EQ(updates[0].name, "Recent Artist");
        EXPECT_EQ(updates[0].sortName, "Recent, Sort");
    }

    TEST_F(DatabaseFixture, Artist_findNameUpdates_trackOriginalDateFirst)
    {
        ScopedArtist artist{ session, "Artist", artistMBID };
        ScopedTrack reissuedTrack{ session };
        ScopedTrack track{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            reissuedTrack.get().modify()->setDate(core::PartialDateTime{ 2020 });
            reissuedTrack.get().modify()->setOriginalDate(core::PartialDateTime{ 1990 });
            track.get().modify()->setDate(core::PartialDateTime{ 2000 });
            createTrackArtistLink(session, track.get(), artist.get(), true, "Artist", "Track, Sort");
            createTrackArtistLink(session, reissuedTrack.get(), artist.get(), true, "Artist", "Reissued, Sort");
        }

        const std::vector<NameUpdate> updates{ findNameUpdates(session) };
        ASSERT_EQ(updates.size(), 1);
        EXPECT_EQ(updates[0].sortName, "Track, Sort");
    }

    TEST_F(DatabaseFixture, Artist_findNameUpdates_releaseDatedByMostRecentTrack)
    {
        ScopedArtist artist{ session, "Artist", artistMBID };
        ScopedRelease release1{ session, "Release1" };
        ScopedRelease release2{ session, "Release2" };
        ScopedTrack release1OldTrack{ session };
        ScopedTrack release1RecentTrack{ session };
        ScopedTrack release2Track{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            release1OldTrack.get().modify()->setRelease(release1.get());
            release1OldTrack.get().modify()->setDate(core::PartialDateTime{ 2000 });
            release1RecentTrack.get().modify()->setRelease(release1.get());
            release1RecentTrack.get().modify()->setDate(core::PartialDateTime{ 2015 });
            release2Track.get().modify()->setRelease(release2.get());
            release2Track.get().modify()->setDate(core::PartialDateTime{ 2010 });
            createReleaseArtistLink(session, release1.get(), artist.get(), true, "Artist", "Release1, Sort");
            createReleaseArtistLink(session, release2.get(), artist.get(), true, "Artist", "Release2, Sort");
        }

        const std::vector<NameUpdate> updates{ findNameUpdates(session) };
        ASSERT_EQ(updates.size(), 1);
        EXPECT_EQ(updates[0].sortName, "Release1, Sort");
    }

    TEST_F(DatabaseFixture, Artist_findNameUpdates_sameDateLastCreatedFirst)
    {
        ScopedArtist artist{ session, "Artist", artistMBID };
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            track1.get().modify()->setDate(core::PartialDateTime{ 2000 });
            track2.get().modify()->setDate(core::PartialDateTime{ 2000 });
            createTrackArtistLink(session, track1.get(), artist.get(), true, "Artist", "First, Sort");
            createTrackArtistLink(session, track2.get(), artist.get(), true, "Artist", "Last, Sort");
        }

        const std::vector<NameUpdate> updates{ findNameUpdates(session) };
        ASSERT_EQ(updates.size(), 1);
        EXPECT_EQ(updates[0].sortName, "Last, Sort");
    }

    TEST_F(DatabaseFixture, Artist_findNameUpdates_sourceWithoutSortNameSkippedForSortNameOnly)
    {
        ScopedArtist artist{ session, "Artist", artistMBID };
        ScopedRelease release{ session, "Release" };
        ScopedTrack track{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            createReleaseArtistLink(session, release.get(), artist.get(), true, "Release Artist", "");
            createTrackArtistLink(session, track.get(), artist.get(), true, "Track Artist", "Track, Sort");
        }

        const std::vector<NameUpdate> updates{ findNameUpdates(session) };
        ASSERT_EQ(updates.size(), 1);
        EXPECT_EQ(updates[0].name, "Release Artist");
        EXPECT_EQ(updates[0].sortName, "Track, Sort");
    }

    TEST_F(DatabaseFixture, Artist_findNameUpdates_artistWithoutMBIDNeverRenamed)
    {
        ScopedArtist artist{ session, "Artist" };

        {
            auto transaction{ session.createWriteTransaction() };
            createArtistInfo(session, artist.get(), false, "Info Artist", "Info, Sort");
        }

        const std::vector<NameUpdate> updates{ findNameUpdates(session) };
        ASSERT_EQ(updates.size(), 1);
        EXPECT_EQ(updates[0].name, "Artist");
        EXPECT_EQ(updates[0].sortName, "Info, Sort");
    }
} // namespace lms::db::tests
