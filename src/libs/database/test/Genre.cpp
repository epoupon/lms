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

namespace lms::db::tests
{
    using ScopedGenre = ScopedEntity<db::Genre>;

    TEST_F(DatabaseFixture, Genre_create)
    {
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Genre::getCount(session), 0);
        }

        ScopedGenre genre{ session, "Rock" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Genre::getCount(session), 1);

            const Genre::pointer found{ Genre::find(session, genre.getId()) };
            ASSERT_TRUE(found);
            EXPECT_EQ(found->getName(), "Rock");
            EXPECT_EQ(found->getTrackCount(), 0);
            EXPECT_EQ(found->getReleaseCount(), 0);
        }
    }

    TEST_F(DatabaseFixture, Genre_findByName)
    {
        ScopedGenre genre{ session, "Jazz" };

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_TRUE(Genre::find(session, "Jazz"));
            EXPECT_FALSE(Genre::find(session, "jazz"));
            EXPECT_FALSE(Genre::find(session, ""));
            EXPECT_FALSE(Genre::find(session, "Jaz"));
        }
    }

    TEST_F(DatabaseFixture, Genre_orphan)
    {
        ScopedGenre genre{ session, "Blues" };

        {
            auto transaction{ session.createReadTransaction() };
            const auto orphans{ Genre::findOrphanIds(session) };
            ASSERT_EQ(orphans.size(), 1);
            EXPECT_EQ(orphans.front(), genre.getId());
        }
    }

    TEST_F(DatabaseFixture, Genre_singleTrack)
    {
        ScopedTrack track{ session };
        ScopedGenre genre1{ session, "Metal" };
        ScopedGenre genre2{ session, "Punk" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Genre::findOrphanIds(session).size(), 2);
            EXPECT_EQ(track->getGenres().size(), 0);
            EXPECT_EQ(track->getGenreIds().size(), 0);
            EXPECT_EQ(Genre::computeTrackCount(session, genre1.getId()), 0);
            EXPECT_EQ(Genre::computeTrackCount(session, genre2.getId()), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setGenres(std::array{ genre1.get() });
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto genres{ Genre::findIds(session, Genre::FindParameters{}.setTrack(track.getId())) };
            ASSERT_EQ(genres.size(), 1);
            EXPECT_EQ(genres.front(), genre1.getId());

            EXPECT_EQ(Genre::computeTrackCount(session, genre1.getId()), 1);
            EXPECT_EQ(Genre::computeTrackCount(session, genre2.getId()), 0);

            const auto orphans{ Genre::findOrphanIds(session) };
            ASSERT_EQ(orphans.size(), 1);
            EXPECT_EQ(orphans.front(), genre2.getId());

            const auto trackGenres{ track->getGenres() };
            ASSERT_EQ(trackGenres.size(), 1);
            EXPECT_EQ(trackGenres.front()->getId(), genre1.getId());

            const auto trackGenreIds{ track->getGenreIds() };
            ASSERT_EQ(trackGenreIds.size(), 1);
            EXPECT_EQ(trackGenreIds.front(), genre1.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto tracks{ Track::findIds(session, Track::FindParameters{}.setFilters(Filters{}.setGenre(genre1.getId()))) };
            ASSERT_EQ(tracks.size(), 1);
            EXPECT_EQ(tracks.front(), track.getId());

            const auto tracks2{ Track::findIds(session, Track::FindParameters{}.setFilters(Filters{}.setGenre(genre2.getId()))) };
            EXPECT_EQ(tracks2.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Genre_multipleGenresOnTrack)
    {
        ScopedTrack track{ session };
        ScopedGenre genre1{ session, "Electronic" };
        ScopedGenre genre2{ session, "Ambient" };

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setGenres(std::array{ genre1.get(), genre2.get() });
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Genre::computeTrackCount(session, genre1.getId()), 1);
            EXPECT_EQ(Genre::computeTrackCount(session, genre2.getId()), 1);
            EXPECT_EQ(Genre::findOrphanIds(session).size(), 0);

            const auto trackGenres{ track->getGenres() };
            EXPECT_EQ(trackGenres.size(), 2);
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto tracks{ Track::findIds(session, Track::FindParameters{}.setFilters(Filters{}.setGenre(genre1.getId()))) };
            ASSERT_EQ(tracks.size(), 1);
            EXPECT_EQ(tracks.front(), track.getId());

            const auto tracks2{ Track::findIds(session, Track::FindParameters{}.setFilters(Filters{}.setGenre(genre2.getId()))) };
            ASSERT_EQ(tracks2.size(), 1);
            EXPECT_EQ(tracks2.front(), track.getId());
        }
    }

    TEST_F(DatabaseFixture, Genre_computeReleaseCount)
    {
        ScopedRelease release{ session, "MyRelease" };
        ScopedTrack track{ session };
        ScopedGenre genre{ session, "Classical" };

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setRelease(release.get());
            track.get().modify()->setGenres(std::array{ genre.get() });
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Genre::computeTrackCount(session, genre.getId()), 1);
            EXPECT_EQ(Genre::computeReleaseCount(session, genre.getId()), 1);
        }
    }

    TEST_F(DatabaseFixture, Genre_nameTruncation)
    {
        const std::string longName(Genre::maxNameLength + 100, 'x');
        const std::string expectedName(Genre::maxNameLength, 'x');

        {
            auto transaction{ session.createWriteTransaction() };
            Genre::pointer genre{ session.create<Genre>(longName) };
            ASSERT_TRUE(genre);
            EXPECT_EQ(genre->getName().size(), Genre::maxNameLength);
            EXPECT_EQ(genre->getName(), expectedName);
            genre.remove();
        }
    }

    TEST_F(DatabaseFixture, Genre_sortByName)
    {
        ScopedGenre g1{ session, "Zzz" };
        ScopedGenre g2{ session, "Aaa" };
        ScopedGenre g3{ session, "Mmm" };

        {
            auto transaction{ session.createReadTransaction() };
            const auto genres{ Genre::findIds(session, Genre::FindParameters{}.setSortMethod(GenreSortMethod::Name)) };
            ASSERT_EQ(genres.size(), 3);
            EXPECT_EQ(genres[0], g2.getId());
            EXPECT_EQ(genres[1], g3.getId());
            EXPECT_EQ(genres[2], g1.getId());
        }
    }

    TEST_F(DatabaseFixture, Genre_sortByTrackCount)
    {
        ScopedGenre g1{ session, "Rock" };
        ScopedGenre g2{ session, "Jazz" };
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedTrack track3{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            track1.get().modify()->setGenres(std::array{ g1.get() });
            track2.get().modify()->setGenres(std::array{ g1.get() });
            track3.get().modify()->setGenres(std::array{ g2.get() });
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto genres{ Genre::findIds(session, Genre::FindParameters{}.setSortMethod(GenreSortMethod::TrackCountDesc)) };
            ASSERT_EQ(genres.size(), 2);
            EXPECT_EQ(genres[0], g1.getId());
            EXPECT_EQ(genres[1], g2.getId());
        }
    }
} // namespace lms::db::tests
