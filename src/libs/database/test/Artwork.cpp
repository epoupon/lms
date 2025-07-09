/*
 * Copyright (C) 2025of LMS.
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

#include "database/objects/Artwork.hpp"
#include "database/objects/Image.hpp"
#include "database/objects/TrackEmbeddedImage.hpp"
#include "database/objects/TrackEmbeddedImageLink.hpp"

namespace lms::db::tests
{
    using ScopedArtwork = ScopedEntity<db::Artwork>;
    using ScopedImage = ScopedEntity<db::Image>;
    using ScopedTrackEmbeddedImage = ScopedEntity<db::TrackEmbeddedImage>;

    TEST_F(DatabaseFixture, Artwork_image)
    {
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Artwork::getCount(session), 0);
        }

        ScopedImage image{ session, "/MyImage" };
        ScopedArtwork artwork{ session, image.lockAndGet() };

        const Wt::WDateTime dateTime{ Wt::WDate{ 2025, 1, 1 } };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Artwork::getCount(session), 1);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            image.get().modify()->setLastWriteTime(dateTime);
            image.get().modify()->setAbsoluteFilePath("/tmp/foo");
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(artwork.get()->getLastWrittenTime(), dateTime);
            EXPECT_EQ(artwork.get()->getAbsoluteFilePath(), "/tmp/foo");
        }
    }

    TEST_F(DatabaseFixture, Artwork_trackEmbeddedImage)
    {
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Artwork::getCount(session), 0);
        }

        ScopedTrackEmbeddedImage image{ session };
        ScopedTrack track{ session };
        ScopedArtwork artwork{ session, image.lockAndGet() };

        const Wt::WDateTime dateTime{ Wt::WDate{ 2025, 1, 1 } };

        {
            auto transaction{ session.createWriteTransaction() };
            session.create<db::TrackEmbeddedImageLink>(track.get(), image.get());
            track.get().modify()->setLastWriteTime(dateTime);
            track.get().modify()->setAbsoluteFilePath("/tmp/foo");
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(artwork.get()->getLastWrittenTime(), dateTime);
            EXPECT_EQ(artwork.get()->getAbsoluteFilePath(), "/tmp/foo");
        }
    }
} // namespace lms::db::tests