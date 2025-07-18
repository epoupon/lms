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

#include "database/objects/ScanSettings.hpp"

#include <initializer_list>

#include "Common.hpp"

namespace lms::db::tests
{
    using ScopedScanSettings = ScopedEntity<db::ScanSettings>;

    TEST_F(DatabaseFixture, ScanSettings)
    {
        ScopedScanSettings settings{ session, "test" };

        {
            auto transaction{ session.createReadTransaction() };
            const auto artists{ settings.get()->getArtistsToNotSplit() };
            ASSERT_EQ(artists.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            settings.get().modify()->setArtistsToNotSplit(std::initializer_list<std::string_view>{ "AC/DC", "My/Artist" });
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto artists{ settings.get()->getArtistsToNotSplit() };
            ASSERT_EQ(artists.size(), 2);
        }
    }
} // namespace lms::db::tests