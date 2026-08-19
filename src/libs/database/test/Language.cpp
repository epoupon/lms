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
    using ScopedLanguage = ScopedEntity<db::Language>;

    TEST_F(DatabaseFixture, Language_create)
    {
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Language::getCount(session), 0);
        }

        ScopedLanguage language{ session, "eng" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Language::getCount(session), 1);

            const Language::pointer found{ Language::find(session, language.getId()) };
            ASSERT_TRUE(found);
            EXPECT_EQ(found->getName(), "eng");
        }
    }

    TEST_F(DatabaseFixture, Language_findByName)
    {
        ScopedLanguage language{ session, "fra" };

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_TRUE(Language::find(session, "fra"));
            EXPECT_FALSE(Language::find(session, "FRA"));
            EXPECT_FALSE(Language::find(session, ""));
            EXPECT_FALSE(Language::find(session, "fr"));
        }
    }

    TEST_F(DatabaseFixture, Language_orphan)
    {
        ScopedLanguage language{ session, "jpn" };

        {
            auto transaction{ session.createReadTransaction() };
            const auto orphans{ Language::findOrphanIds(session) };
            ASSERT_EQ(orphans.size(), 1);
            EXPECT_EQ(orphans.front(), language.getId());
        }
    }

    TEST_F(DatabaseFixture, Language_singleTrack)
    {
        ScopedTrack track{ session };
        ScopedLanguage language1{ session, "eng" };
        ScopedLanguage language2{ session, "spa" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Language::findOrphanIds(session).size(), 2);
            EXPECT_EQ(track->getLanguages().size(), 0);
            EXPECT_EQ(track->getLanguageIds().size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setLanguages(std::array{ language1.get() });
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto languages{ Language::findIds(session, Language::FindParameters{}.setTrack(track.getId())) };
            ASSERT_EQ(languages.size(), 1);
            EXPECT_EQ(languages.front(), language1.getId());

            const auto orphans{ Language::findOrphanIds(session) };
            ASSERT_EQ(orphans.size(), 1);
            EXPECT_EQ(orphans.front(), language2.getId());

            const auto trackLanguages{ track->getLanguages() };
            ASSERT_EQ(trackLanguages.size(), 1);
            EXPECT_EQ(trackLanguages.front()->getId(), language1.getId());

            const auto trackLanguageIds{ track->getLanguageIds() };
            ASSERT_EQ(trackLanguageIds.size(), 1);
            EXPECT_EQ(trackLanguageIds.front(), language1.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto tracks{ Track::findIds(session, Track::FindParameters{}.setFilters(Filters{}.setLanguage(language1.getId()))) };
            ASSERT_EQ(tracks.size(), 1);
            EXPECT_EQ(tracks.front(), track.getId());

            const auto tracks2{ Track::findIds(session, Track::FindParameters{}.setFilters(Filters{}.setLanguage(language2.getId()))) };
            EXPECT_EQ(tracks2.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Language_multipleLanguagesOnTrack)
    {
        ScopedTrack track{ session };
        ScopedLanguage language1{ session, "eng" };
        ScopedLanguage language2{ session, "fra" };

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setLanguages(std::array{ language1.get(), language2.get() });
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Language::findOrphanIds(session).size(), 0);

            const auto trackLanguages{ track->getLanguages() };
            EXPECT_EQ(trackLanguages.size(), 2);
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto tracks{ Track::findIds(session, Track::FindParameters{}.setFilters(Filters{}.setLanguage(language1.getId()))) };
            ASSERT_EQ(tracks.size(), 1);
            EXPECT_EQ(tracks.front(), track.getId());

            const auto tracks2{ Track::findIds(session, Track::FindParameters{}.setFilters(Filters{}.setLanguage(language2.getId()))) };
            ASSERT_EQ(tracks2.size(), 1);
            EXPECT_EQ(tracks2.front(), track.getId());
        }
    }

    TEST_F(DatabaseFixture, Language_sortByName)
    {
        ScopedLanguage l1{ session, "zho" };
        ScopedLanguage l2{ session, "ara" };
        ScopedLanguage l3{ session, "por" };

        {
            auto transaction{ session.createReadTransaction() };
            const auto languages{ Language::findIds(session, Language::FindParameters{}.setSortMethod(LanguageSortMethod::Name)) };
            ASSERT_EQ(languages.size(), 3);
            EXPECT_EQ(languages[0], l2.getId());
            EXPECT_EQ(languages[1], l3.getId());
            EXPECT_EQ(languages[2], l1.getId());
        }
    }

    TEST_F(DatabaseFixture, Language_sortByTrackCount)
    {
        ScopedLanguage l1{ session, "eng" };
        ScopedLanguage l2{ session, "fra" };
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedTrack track3{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            track1.get().modify()->setLanguages(std::array{ l1.get() });
            track2.get().modify()->setLanguages(std::array{ l1.get() });
            track3.get().modify()->setLanguages(std::array{ l2.get() });
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto languages{ Language::findIds(session, Language::FindParameters{}.setSortMethod(LanguageSortMethod::TrackCountDesc)) };
            ASSERT_EQ(languages.size(), 2);
            EXPECT_EQ(languages[0], l1.getId());
            EXPECT_EQ(languages[1], l2.getId());
        }
    }
} // namespace lms::db::tests
