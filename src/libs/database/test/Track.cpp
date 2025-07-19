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

#include <algorithm>

#include "database/objects/Artwork.hpp"
#include "database/objects/Image.hpp"

namespace lms::db::tests
{
    using ScopedArtwork = ScopedEntity<db::Artwork>;
    using ScopedImage = ScopedEntity<db::Image>;

    TEST_F(DatabaseFixture, Track)
    {
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Track::find(session, Track::FindParameters{}).results.size(), 0);
            EXPECT_EQ(Track::findIds(session, Track::FindParameters{}).results.size(), 0);
            EXPECT_EQ(Track::getCount(session), 0);
            EXPECT_FALSE(Track::exists(session, 0));

            {
                bool visited{};
                Track::find(session, Track::FindParameters{}, [&](const Track::pointer&) { visited = true; });
                EXPECT_FALSE(visited);
            }
        }

        ScopedTrack track{ session };

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(Track::find(session, Track::FindParameters{}).results.size(), 1);
            EXPECT_EQ(Track::getCount(session), 1);
            EXPECT_TRUE(Track::exists(session, track.getId()));
            auto myTrack{ Track::find(session, track.getId()) };
            ASSERT_TRUE(myTrack);
            EXPECT_EQ(myTrack->getId(), track.getId());

            {
                bool visited{};
                Track::find(session, Track::FindParameters{}, [&](const Track::pointer& t) {
                    visited = true;
                    EXPECT_EQ(t->getId(), track.getId());
                });
                EXPECT_TRUE(visited);
            }
        }
    }

    TEST_F(DatabaseFixture, Track_findByRangedIdBased)
    {
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedTrack track3{ session };
        ScopedMediaLibrary library{ session, "MyLibrary", "/root" };
        ScopedMediaLibrary otherLibrary{ session, "OtherLibrary", "/otherRoot" };

        {
            auto transaction{ session.createWriteTransaction() };
            track2.get().modify()->setMediaLibrary(library.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            TrackId lastRetrievedTrackId;
            std::vector<Track::pointer> visitedTracks;
            Track::find(session, lastRetrievedTrackId, 10, [&](const Track::pointer& track) {
                visitedTracks.push_back(track);
            });
            ASSERT_EQ(visitedTracks.size(), 3);
            EXPECT_EQ(visitedTracks[0]->getId(), track1.getId());
            EXPECT_EQ(visitedTracks[1]->getId(), track2.getId());
            EXPECT_EQ(visitedTracks[2]->getId(), track3.getId());
            EXPECT_EQ(lastRetrievedTrackId, track3.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            TrackId lastRetrievedTrackId{ track1.getId() };
            std::vector<Track::pointer> visitedTracks;
            Track::find(session, lastRetrievedTrackId, 1, [&](const Track::pointer& track) {
                visitedTracks.push_back(track);
            });
            ASSERT_EQ(visitedTracks.size(), 1);
            EXPECT_EQ(visitedTracks[0]->getId(), track2.getId());
            EXPECT_EQ(lastRetrievedTrackId, track2.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            TrackId lastRetrievedTrackId{ track1.getId() };
            std::vector<Track::pointer> visitedTracks;
            Track::find(session, lastRetrievedTrackId, 0, [&](const Track::pointer& track) {
                visitedTracks.push_back(track);
            });
            ASSERT_EQ(visitedTracks.size(), 0);
            EXPECT_EQ(lastRetrievedTrackId, track1.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            TrackId lastRetrievedTrackId{};
            std::vector<Track::pointer> visitedTracks;
            Track::find(
                session, lastRetrievedTrackId, 10, [&](const Track::pointer& track) {
                    visitedTracks.push_back(track);
                },
                otherLibrary.getId());
            ASSERT_EQ(visitedTracks.size(), 0);
            EXPECT_EQ(lastRetrievedTrackId, TrackId{});
        }

        {
            auto transaction{ session.createReadTransaction() };

            TrackId lastRetrievedTrackId{};
            std::vector<Track::pointer> visitedTracks;
            Track::find(
                session, lastRetrievedTrackId, 10, [&](const Track::pointer& track) {
                    visitedTracks.push_back(track);
                },
                library.getId());
            ASSERT_EQ(visitedTracks.size(), 1);
            EXPECT_EQ(visitedTracks[0]->getId(), track2.getId());
            EXPECT_EQ(lastRetrievedTrackId, track2.getId());
        }
    }

    TEST_F(DatabaseFixture, Track_findNextIdRange)
    {
        {
            auto transaction{ session.createReadTransaction() };

            auto range{ Track::findNextIdRange(session, TrackId{}, 0) };
            EXPECT_FALSE(range.isValid());
            EXPECT_EQ(range.first, TrackId{});
            EXPECT_EQ(range.last, TrackId{});

            range = Track::findNextIdRange(session, TrackId{}, 100);
            EXPECT_FALSE(range.isValid());
            EXPECT_EQ(range.first, TrackId{});
            EXPECT_EQ(range.last, TrackId{});
        }

        ScopedTrack track1{ session };
        {
            auto transaction{ session.createReadTransaction() };

            auto range{ Track::findNextIdRange(session, TrackId{}, 0) };
            EXPECT_FALSE(range.isValid());
            EXPECT_EQ(range.first, TrackId{});
            EXPECT_EQ(range.last, TrackId{});

            range = Track::findNextIdRange(session, TrackId{}, 1);
            EXPECT_TRUE(range.isValid());
            EXPECT_EQ(range.first, track1.getId());
            EXPECT_EQ(range.last, track1.getId());

            range = Track::findNextIdRange(session, range.last, 1);
            EXPECT_FALSE(range.isValid());
            EXPECT_EQ(range.first, TrackId{});
            EXPECT_EQ(range.last, TrackId{});

            range = Track::findNextIdRange(session, TrackId{}, 100);
            EXPECT_TRUE(range.isValid());
            EXPECT_EQ(range.first, track1.getId());
            EXPECT_EQ(range.last, track1.getId());
        }

        ScopedTrack track2{ session };
        ScopedTrack track3{ session };

        {
            auto transaction{ session.createReadTransaction() };

            auto range{ Track::findNextIdRange(session, TrackId{}, 2) };
            EXPECT_TRUE(range.isValid());
            EXPECT_EQ(range.first, track1.getId());
            EXPECT_EQ(range.last, track2.getId());

            range = Track::findNextIdRange(session, track2.getId(), 2);
            EXPECT_TRUE(range.isValid());
            EXPECT_EQ(range.first, track3.getId());
            EXPECT_EQ(range.last, track3.getId());
        }
    }

    TEST_F(DatabaseFixture, Track_findByRange)
    {
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedTrack track3{ session };

        {
            auto transaction{ session.createReadTransaction() };

            std::size_t count{};
            Track::find(session, IdRange<TrackId>{ .first = track1.getId(), .last = track1.getId() }, [&](const db::Track::pointer& track) {
                count++;
                EXPECT_EQ(track->getId(), track1.getId());
            });
            EXPECT_EQ(count, 1);
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::size_t count{};
            Track::find(session, IdRange<TrackId>{ .first = track1.getId(), .last = track3.getId() }, [&](const db::Track::pointer&) {
                count++;
            });
            EXPECT_EQ(count, 3);
        }
    }

    TEST_F(DatabaseFixture, Track_findAbsoluteFilePath)
    {
        ScopedTrack track{ session };
        const std::filesystem::path absoluteFilePath{ "/path/to/track.mp3" };
        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setAbsoluteFilePath(absoluteFilePath);
        }

        {
            auto transaction{ session.createReadTransaction() };

            TrackId lastRetrievedTrackId;
            std::vector<std::pair<TrackId, std::filesystem::path>> visitedTracks;
            Track::findAbsoluteFilePath(session, lastRetrievedTrackId, 10, [&](TrackId trackId, const std::filesystem::path& filePath) {
                visitedTracks.emplace_back(trackId, filePath);
            });
            ASSERT_EQ(visitedTracks.size(), 1);
            EXPECT_EQ(visitedTracks[0].first, track.getId());
            EXPECT_EQ(visitedTracks[0].second, absoluteFilePath);
            EXPECT_EQ(lastRetrievedTrackId, track.getId());
        }
    }

    TEST_F(DatabaseFixture, Track_MediaLibrary)
    {
        ScopedTrack track{ session };
        ScopedMediaLibrary library{ session, "MyLibrary", "/root" };
        ScopedMediaLibrary otherLibrary{ session, "OtherLibrary", "/otherRoot" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_TRUE(library->isEmpty());
            EXPECT_TRUE(otherLibrary->isEmpty());
        }

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setMediaLibrary(library.get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_FALSE(library->isEmpty());
            EXPECT_TRUE(otherLibrary->isEmpty());
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto tracks{ Track::findIds(session, Track::FindParameters{}.setFilters(Filters{}.setMediaLibrary(library->getId()))) };
            ASSERT_EQ(tracks.results.size(), 1);
            EXPECT_EQ(tracks.results.front(), track.getId());
        }
        {
            auto transaction{ session.createReadTransaction() };
            const auto tracks{ Track::findIds(session, Track::FindParameters{}.setFilters(Filters{}.setMediaLibrary(otherLibrary->getId()))) };
            EXPECT_EQ(tracks.results.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Track_noMediaLibrary)
    {
        ScopedTrack track{ session };
        {
            auto transaction{ session.createReadTransaction() };
            MediaLibrary::pointer mediaLibrary{ track->getMediaLibrary() };
            EXPECT_EQ(mediaLibrary, MediaLibrary::pointer{});
            EXPECT_FALSE(mediaLibrary);
            EXPECT_TRUE(!mediaLibrary);
        }
    }

    TEST_F(DatabaseFixture, TrackNotExists)
    {
        auto transaction{ session.createReadTransaction() };

        EXPECT_FALSE(Track::exists(session, TrackId{ 42 }));
        EXPECT_EQ(Track::find(session, TrackId{ 42 }), Track::pointer{});
        EXPECT_FALSE(Track::find(session, TrackId{ 42 }));
        EXPECT_EQ(Track::find(session, Track::FindParameters{}).results.size(), 0);
        {
            auto track{ Track::find(session, TrackId{ 42 }) };
            EXPECT_TRUE(!track);
            EXPECT_FALSE(track);
        }
    }

    TEST_F(DatabaseFixture, MultipleTracks)
    {
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_TRUE(track1.getId() != track2.getId());
            EXPECT_TRUE(track1.get() != track2.get());
            EXPECT_FALSE(track1.get() == track2.get());
        }
    }

    TEST_F(DatabaseFixture, MultipleTracksSearchByFilter)
    {
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedTrack track3{ session };
        ScopedTrack track4{ session };
        ScopedTrack track5{ session };
        ScopedTrack track6{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            track1.get().modify()->setName("MyTrack");
            track2.get().modify()->setName("MyTrack%");
            track3.get().modify()->setName("MyTrack%Foo");
            track4.get().modify()->setName("%MyTrack");
            track5.get().modify()->setName("Foo%MyTrack");
            track6.get().modify()->setName("M_Track");
        }

        {
            auto transaction{ session.createReadTransaction() };

            {
                const auto tracks{ Track::findIds(session, Track::FindParameters{}.setKeywords({ "Track" })) };
                EXPECT_EQ(tracks.results.size(), 6);
            }
            {
                const auto tracks{ Track::findIds(session, Track::FindParameters{}.setKeywords({ "MyTrack" })) };
                EXPECT_EQ(tracks.results.size(), 5);
                EXPECT_TRUE(std::none_of(std::cbegin(tracks.results), std::cend(tracks.results), [&](const TrackId trackId) { return trackId == track6.getId(); }));
            }
            {
                const auto tracks{ Track::findIds(session, Track::FindParameters{}.setKeywords({ "MyTrack%" })) };
                ASSERT_EQ(tracks.results.size(), 2);
                EXPECT_EQ(tracks.results[0], track2.getId());
                EXPECT_EQ(tracks.results[1], track3.getId());
            }
            {
                const auto tracks{ Track::findIds(session, Track::FindParameters{}.setKeywords({ "%MyTrack" })) };
                ASSERT_EQ(tracks.results.size(), 2);
                EXPECT_EQ(tracks.results[0], track4.getId());
                EXPECT_EQ(tracks.results[1], track5.getId());
            }
        }
    }

    TEST_F(DatabaseFixture, Track_date)
    {
        ScopedTrack track{ session };
        const core::PartialDateTime date{ 1995, 5, 5 };
        const core::PartialDateTime originalDate{ 1994, 2, 2 };
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(track->getYear(), std::nullopt);
            EXPECT_EQ(track->getOriginalYear(), std::nullopt);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setDate(date);
            track.get().modify()->setOriginalDate(originalDate);
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(track->getYear(), 1995);
            EXPECT_EQ(track->getOriginalYear(), 1994);
            EXPECT_EQ(track->getDate(), date);
            EXPECT_EQ(track->getOriginalDate(), originalDate);
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(track->getYear(), date.getYear());
            EXPECT_EQ(track->getOriginalYear(), originalDate.getYear());
        }
    }

    TEST_F(DatabaseFixture, Track_writtenAfter)
    {
        ScopedTrack track{ session };

        const Wt::WDateTime dateTime{ Wt::WDate{ 1950, 1, 1 }, Wt::WTime{ 12, 30, 20 } };

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setLastWriteTime(dateTime);
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto tracks{ Track::findIds(session, Track::FindParameters{}) };
            EXPECT_EQ(tracks.results.size(), 1);
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto tracks{ Track::findIds(session, Track::FindParameters{}.setWrittenAfter(dateTime.addSecs(-1))) };
            EXPECT_EQ(tracks.results.size(), 1);
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto tracks{ Track::findIds(session, Track::FindParameters{}.setWrittenAfter(dateTime.addSecs(+1))) };
            EXPECT_EQ(tracks.results.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Track_path)
    {
        ScopedTrack track{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setAbsoluteFilePath("/root/foo/file.path");
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(track->getAbsoluteFilePath(), "/root/foo/file.path");
        }
    }

    TEST_F(DatabaseFixture, Track_audioProperties)
    {
        ScopedTrack track{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setBitrate(128000);
            track.get().modify()->setBitsPerSample(16);
            track.get().modify()->setDuration(std::chrono::minutes{ 3 });
            track.get().modify()->setChannelCount(2);
            track.get().modify()->setSampleRate(44100);
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(track->getBitrate(), 128000);
            EXPECT_EQ(track->getBitsPerSample(), 16);
            EXPECT_EQ(track->getDuration(), std::chrono::minutes{ 3 });
            EXPECT_EQ(track->getChannelCount(), 2);
            EXPECT_EQ(track->getSampleRate(), 44100);
        }
    }

    TEST_F(DatabaseFixture, Track_comment)
    {
        ScopedTrack track{ session };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(track->getComment(), "");
        }

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setComment("MyComment");
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(track->getComment(), "MyComment");
        }
    }

    TEST_F(DatabaseFixture, Track_sortDateAdded)
    {
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedTrack track3{ session };
        ScopedTrack track4{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            track1.get().modify()->setAddedTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 2 } });
            track2.get().modify()->setAddedTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 1 } });
            track4.get().modify()->setAddedTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 2 }, Wt::WTime{ 15, 36, 24 } });
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto tracks{ Track::findIds(session, Track::FindParameters{}.setSortMethod(TrackSortMethod::AddedDesc)) };
            ASSERT_EQ(tracks.results.size(), 4);
            EXPECT_EQ(tracks.results[0], track4.getId());
            EXPECT_EQ(tracks.results[1], track1.getId());
            EXPECT_EQ(tracks.results[2], track2.getId());
            EXPECT_EQ(tracks.results[3], track3.getId());
        }
    }

    TEST_F(DatabaseFixture, Track_sortLastWritten)
    {
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedTrack track3{ session };
        ScopedTrack track4{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            track1.get().modify()->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 2 } });
            track2.get().modify()->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 1 } });
            track4.get().modify()->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 2 }, Wt::WTime{ 15, 36, 24 } });
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto tracks{ Track::findIds(session, Track::FindParameters{}.setSortMethod(TrackSortMethod::LastWrittenDesc)) };
            ASSERT_EQ(tracks.results.size(), 4);
            EXPECT_EQ(tracks.results[0], track4.getId());
            EXPECT_EQ(tracks.results[1], track1.getId());
            EXPECT_EQ(tracks.results[2], track2.getId());
            EXPECT_EQ(tracks.results[3], track3.getId());
        }
    }

    TEST_F(DatabaseFixture, Track_updateArtworks)
    {
        ScopedTrack track{ session };
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(track->getPreferredArtwork(), Artwork::pointer{});
            EXPECT_EQ(track->getPreferredMediaArtwork(), Artwork::pointer{});
        }

        ScopedImage image1{ session, "/image1.jpg" };
        ScopedArtwork artwork1{ session, image1.lockAndGet() };
        ScopedImage image2{ session, "/image2.jpg" };
        ScopedArtwork artwork2{ session, image2.lockAndGet() };

        {
            auto transaction{ session.createWriteTransaction() };
            Track::updatePreferredArtwork(session, track->getId(), artwork1->getId());
            Track::updatePreferredMediaArtwork(session, track->getId(), artwork2->getId());
        }
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(track->getPreferredArtwork()->getId(), artwork1->getId());
            EXPECT_EQ(track->getPreferredMediaArtwork()->getId(), artwork2->getId());
        }

        {
            auto transaction{ session.createWriteTransaction() };
            Track::updatePreferredArtwork(session, track->getId(), ArtworkId{});
            Track::updatePreferredMediaArtwork(session, track->getId(), ArtworkId{});
        }
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(track->getPreferredArtwork(), Artwork::pointer{});
            EXPECT_EQ(track->getPreferredMediaArtwork(), Artwork::pointer{});
        }
    }
} // namespace lms::db::tests