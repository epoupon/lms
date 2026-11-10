/*
 * Copyright (C) 2019 Emeric Poupon
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

#include <list>

#include "database/objects/Release.hpp"
#include "database/objects/TrackList.hpp"

#include "Common.hpp"

namespace lms::db::tests
{
    namespace
    {
        std::vector<TrackListEntry> findEntries(ISession& session, TrackListId trackListId, std::optional<Range> range = std::nullopt)
        {
            std::vector<TrackListEntry> entries;
            TrackListEntry::find(session, TrackListEntry::FindParameters{}.setTrackList(trackListId).setRange(range), [&](const TrackListEntry& entry) {
                entries.push_back(entry);
            });
            return entries;
        }
    } // namespace

    using ScopedReleaseType = ScopedEntity<db::ReleaseType>;

    TEST_F(DatabaseFixture, SingleTrackList)
    {
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(TrackList::getCount(session), 0);
        }

        ScopedTrackList trackList{ session, "MytrackList", TrackListType::PlayList };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(TrackList::getCount(session), 1);
        }
    }

    TEST_F(DatabaseFixture, SingleTrackListSingleTrack)
    {
        ScopedTrackList trackList1{ session, "MyTrackList1", TrackListType::PlayList };
        ScopedTrackList trackList2{ session, "MyTrackList2", TrackListType::PlayList };
        ScopedTrack track{ session };

        {
            auto transaction{ session.createReadTransaction() };

            auto tracks{ Track::findIds(session, Track::FindParameters{}.setTrackList(trackList1.getId())) };
            EXPECT_EQ(tracks.size(), 0);

            tracks = Track::findIds(session, Track::FindParameters{}.setTrackList(trackList2.getId()));
            EXPECT_EQ(tracks.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            TrackListEntry::create(session, track.getId(), trackList1.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto tracks{ Track::findIds(session, Track::FindParameters{}.setTrackList(trackList1.getId())) };
            ASSERT_EQ(tracks.size(), 1);
            EXPECT_EQ(tracks.front(), track.getId());

            tracks = Track::findIds(session, Track::FindParameters{}.setTrackList(trackList2.getId()));
            EXPECT_EQ(tracks.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, TrackList_getTrackIds_orderedByPosition)
    {
        ScopedTrackList trackList{ session, "MyTrackList", TrackListType::PlayList };
        ScopedTrackList otherTrackList{ session, "MyOtherTrackList", TrackListType::PlayList };
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedTrack track3{ session };

        {
            auto transaction{ session.createWriteTransaction() };

            TrackListEntry::create(session, track3.getId(), trackList.getId());
            TrackListEntry::create(session, track1.getId(), trackList.getId());
            TrackListEntry::create(session, track3.getId(), trackList.getId());
            TrackListEntry::create(session, track2.getId(), otherTrackList.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto tracks{ Track::findIds(session, Track::FindParameters{}.setTrackList(trackList.getId())) };
            ASSERT_EQ(tracks.size(), 3);
            EXPECT_EQ(tracks[0], track3.getId());
            EXPECT_EQ(tracks[1], track1.getId());
            EXPECT_EQ(tracks[2], track3.getId());

            const auto page{ Track::findIds(session, Track::FindParameters{}.setTrackList(trackList.getId()).setRange(Range{ 1, 1 })) };
            ASSERT_EQ(page.size(), 1);
            EXPECT_EQ(page[0], track1.getId());

            EXPECT_EQ(Track::findIds(session, Track::FindParameters{}.setTrackList(otherTrackList.getId())).size(), 1);
        }
    }

    TEST_F(DatabaseFixture, TrackList_visitTracks_orderedByPosition)
    {
        ScopedTrackList trackList{ session, "MyTrackList", TrackListType::PlayList };
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };

        {
            auto transaction{ session.createReadTransaction() };

            std::size_t count{};
            Track::find(session, Track::FindParameters{}.setTrackList(trackList.getId()), [&](const Track&) { ++count; });
            EXPECT_EQ(count, 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            TrackListEntry::create(session, track2.getId(), trackList.getId());
            TrackListEntry::create(session, track1.getId(), trackList.getId());
            TrackListEntry::create(session, track2.getId(), trackList.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::vector<TrackId> visited;
            Track::find(session, Track::FindParameters{}.setTrackList(trackList.getId()), [&](const Track& track) { visited.push_back(track.getId()); });
            ASSERT_EQ(visited.size(), 3);
            EXPECT_EQ(visited[0], track2.getId());
            EXPECT_EQ(visited[1], track1.getId());
            EXPECT_EQ(visited[2], track2.getId());
        }
    }

    TEST_F(DatabaseFixture, TrackList_SortMethod)
    {
        ScopedTrackList trackList2{ session, "MyTrackList2", TrackListType::PlayList };
        ScopedTrackList trackList1{ session, "MyTrackList1", TrackListType::PlayList };
        ScopedTrack track{ session };

        {
            auto transaction{ session.createReadTransaction() };

            const auto trackLists{ TrackList::find(session, TrackList::FindParameters{}.setSortMethod(TrackListSortMethod::Name)) };
            ASSERT_EQ(trackLists.size(), 2);
            EXPECT_EQ(trackLists[0], trackList1.getId());
            EXPECT_EQ(trackLists[1], trackList2.getId());
        }

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<TrackList> updatedTrackList1{ trackList1.get() };
            updatedTrackList1->setLastModifiedDateTime(Wt::WDateTime{ Wt::WDate{ 1900, 1, 1 } });
            TrackList::update(session, *updatedTrackList1);
            std::optional<TrackList> updatedTrackList2{ trackList2.get() };
            updatedTrackList2->setLastModifiedDateTime(Wt::WDateTime{ Wt::WDate{ 1900, 1, 2 } });
            TrackList::update(session, *updatedTrackList2);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto trackLists{ TrackList::find(session, TrackList::FindParameters{}.setSortMethod(TrackListSortMethod::LastModifiedDesc)) };
            ASSERT_EQ(trackLists.size(), 2);
            EXPECT_EQ(trackLists[0], trackList2.getId());
            EXPECT_EQ(trackLists[1], trackList1.getId());
        }

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<TrackList> updatedTrackList1{ trackList1.get() };
            updatedTrackList1->setLastModifiedDateTime(Wt::WDateTime{ Wt::WDate{ 1900, 1, 2 } });
            TrackList::update(session, *updatedTrackList1);
            std::optional<TrackList> updatedTrackList2{ trackList2.get() };
            updatedTrackList2->setLastModifiedDateTime(Wt::WDateTime{ Wt::WDate{ 1900, 1, 1 } });
            TrackList::update(session, *updatedTrackList2);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto trackLists{ TrackList::find(session, TrackList::FindParameters{}.setSortMethod(TrackListSortMethod::LastModifiedDesc)) };
            ASSERT_EQ(trackLists.size(), 2);
            EXPECT_EQ(trackLists[0], trackList1.getId());
            EXPECT_EQ(trackLists[1], trackList2.getId());
        }
    }

    TEST_F(DatabaseFixture, SingleTrackListMultipleTrack)
    {
        ScopedTrackList trackList{ session, "MytrackList", TrackListType::PlayList };
        std::list<ScopedTrack> tracks;

        for (std::size_t i{}; i < 10; ++i)
        {
            tracks.emplace_back(session);

            auto transaction{ session.createWriteTransaction() };
            TrackListEntry::create(session, tracks.back().getId(), trackList.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            ASSERT_EQ(TrackList::getTrackCount(session, trackList.getId()), tracks.size());
            const auto trackIds{ Track::findIds(session, Track::FindParameters{}.setTrackList(trackList.getId())) };
            ASSERT_EQ(trackIds.size(), tracks.size());

            // Same order
            std::size_t i{};
            for (const ScopedTrack& track : tracks)
                EXPECT_EQ(track.getId(), trackIds[i++]);
        }
    }

    TEST_F(DatabaseFixture, TrackList_MediaLibrary)
    {
        ScopedTrackList trackList1{ session, "MytrackList1", TrackListType::PlayList };
        ScopedTrackList trackList2{ session, "MytrackList2", TrackListType::PlayList };
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedMediaLibrary library{ session, "MyLibrary", "/root" };

        {
            auto transaction{ session.createWriteTransaction() };
            TrackListEntry::create(session, track1.getId(), trackList1.getId());
            TrackListEntry::create(session, track2.getId(), trackList2.getId());
            std::optional<Track> updatedTrack2{ track2.get() };
            updatedTrack2->setMediaLibrary(library.getId());
            Track::update(session, *updatedTrack2);
        }

        {
            auto transaction{ session.createReadTransaction() };
            std::vector<TrackListId> visitedTrackLists;
            TrackList::find(session, TrackList::FindParameters{}, [&](const TrackList& trackList) {
                visitedTrackLists.push_back(trackList.getId());
            });
            ASSERT_EQ(visitedTrackLists.size(), 2);
            EXPECT_EQ(visitedTrackLists[0], trackList1->getId());
            EXPECT_EQ(visitedTrackLists[1], trackList2->getId());
        }

        {
            auto transaction{ session.createReadTransaction() };
            std::vector<TrackListId> visitedTrackLists;
            TrackList::find(session, TrackList::FindParameters{}.setFilter(FilterExpr::equal(library->getId())), [&](const TrackList& trackList) {
                visitedTrackLists.push_back(trackList.getId());
            });
            ASSERT_EQ(visitedTrackLists.size(), 1);
            EXPECT_EQ(visitedTrackLists[0], trackList2->getId());
        }
    }

    TEST_F(DatabaseFixture, TrackList_ReleaseType)
    {
        ScopedTrackList trackList1{ session, "MytrackList1", TrackListType::PlayList };
        ScopedTrackList trackList2{ session, "MytrackList2", TrackListType::PlayList };
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedReleaseType releaseType1{ session, "MyReleaseType1" };
        ScopedReleaseType releaseType2{ session, "MyReleaseType2" };
        ScopedRelease release1{ session, "MyRelease1" };
        ScopedRelease release2{ session, "MyRelease2" };

        {
            auto transaction{ session.createWriteTransaction() };

            TrackListEntry::create(session, track1.getId(), trackList1.getId());
            Release::addReleaseType(session, release1.getId(), releaseType1.getId());
            std::optional<Track> updatedTrack1{ track1.get() };
            updatedTrack1->setRelease(release1.getId());
            Track::update(session, *updatedTrack1);

            TrackListEntry::create(session, track2.getId(), trackList2.getId());
            Release::addReleaseType(session, release2.getId(), releaseType2.getId());
            std::optional<Track> updatedTrack2{ track2.get() };
            updatedTrack2->setRelease(release2.getId());
            Track::update(session, *updatedTrack2);
        }

        {
            auto transaction{ session.createReadTransaction() };
            std::vector<TrackListId> visitedTrackLists;
            TrackList::find(session, TrackList::FindParameters{}.setFilter(FilterExpr::equal(releaseType1->getId())), [&](const TrackList& trackList) {
                visitedTrackLists.push_back(trackList.getId());
            });
            ASSERT_EQ(visitedTrackLists.size(), 1);
            EXPECT_EQ(visitedTrackLists[0], trackList1->getId());
        }

        {
            auto transaction{ session.createReadTransaction() };
            std::vector<TrackListId> visitedTrackLists;
            TrackList::find(session, TrackList::FindParameters{}.setFilter(FilterExpr::equal(releaseType2->getId())), [&](const TrackList& trackList) {
                visitedTrackLists.push_back(trackList.getId());
            });
            ASSERT_EQ(visitedTrackLists.size(), 1);
            EXPECT_EQ(visitedTrackLists[0], trackList2->getId());
        }
    }

    TEST_F(DatabaseFixture, SingleTrackListSingleTrackWithCluster)
    {
        ScopedTrackList trackList1{ session, "MyTrackList1", TrackListType::PlayList };
        ScopedTrackList trackList2{ session, "MyTrackList2", TrackListType::PlayList };
        ScopedClusterType clusterType{ session, "MyClusterType" };
        ScopedCluster cluster{ session, clusterType.getId(), "MyCluster" };
        ScopedTrack track{ session };

        {
            auto transaction{ session.createReadTransaction() };

            auto trackLists{ TrackList::find(session, TrackList::FindParameters{}.setFilter(FilterExpr::equal(cluster.getId()))) };
            EXPECT_EQ(trackLists.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            TrackListEntry::create(session, track.getId(), trackList1.getId());
            Cluster::addTrack(session, cluster.getId(), track.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto trackLists{ TrackList::find(session, TrackList::FindParameters{}.setFilter(FilterExpr::equal(cluster.getId()))) };
            ASSERT_EQ(trackLists.size(), 1);
            EXPECT_EQ(trackLists.front(), trackList1.getId());
        }
    }

    TEST_F(DatabaseFixture, SingleTrackList_getEntries)
    {
        ScopedTrackList trackList{ session, "MyTrackList", TrackListType::PlayList };
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            TrackListEntry::create(session, track1.getId(), trackList.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto entries{ findEntries(session, trackList.getId()) };
            ASSERT_EQ(entries.size(), 1);
            EXPECT_EQ(entries.front().getTrackId(), track1.getId());
        }

        {
            auto transaction{ session.createWriteTransaction() };
            TrackListEntry::create(session, track2.getId(), trackList.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto entries{ findEntries(session, trackList.getId()) };
            ASSERT_EQ(entries.size(), 2);
            EXPECT_EQ(entries[0].getTrackId(), track1.getId());
            EXPECT_EQ(entries[1].getTrackId(), track2.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto entries{ findEntries(session, trackList.getId(), Range{ 1, 1 }) };
            ASSERT_EQ(entries.size(), 1);
            EXPECT_EQ(entries[0].getTrackId(), track2.getId());
        }
    }

    TEST_F(DatabaseFixture, SingleTrackList_visitEntries)
    {
        ScopedTrackList trackList{ session, "MyTrackList", TrackListType::PlayList };
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            TrackListEntry::create(session, track1.getId(), trackList.getId());
            TrackListEntry::create(session, track2.getId(), trackList.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::vector<TrackId> visitedTrackIds;
            TrackListEntry::find(session, TrackListEntry::FindParameters{}.setTrackList(trackList.getId()), [&](const TrackListEntry& entry) {
                visitedTrackIds.push_back(entry.getTrackId());
            });
            ASSERT_EQ(visitedTrackIds.size(), 2);
            EXPECT_EQ(visitedTrackIds[0], track1.getId());
            EXPECT_EQ(visitedTrackIds[1], track2.getId());
        }
    }

} // namespace lms::db::tests