/*
 * Copyright (C) 2024 Emeric Poupon
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

#include <limits>

#include "database/Types.hpp"
#include "database/objects/Medium.hpp"
#include "database/objects/TrackEmbeddedImage.hpp"
#include "database/objects/TrackEmbeddedImageLink.hpp"
#include "database/objects/TrackList.hpp"

#include "Common.hpp"

namespace lms::db::tests
{
    using ScopedMedium = ScopedEntity<db::Medium>;
    using ScopedTrackEmbeddedImage = ScopedEntity<db::TrackEmbeddedImage>;
    using ScopedTrackEmbeddedImageLink = ScopedEntity<db::TrackEmbeddedImageLink>;

    TEST_F(DatabaseFixture, TrackEmbeddedImage)
    {
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(TrackEmbeddedImage::getCount(session), 0);
        }

        ScopedTrackEmbeddedImage image{ session };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(TrackEmbeddedImage::getCount(session), 1);

            const TrackEmbeddedImage::pointer img{ TrackEmbeddedImage::find(session, image.getId()) };
            ASSERT_NE(img, TrackEmbeddedImage::pointer{});
            EXPECT_EQ(img->getHash(), db::ImageHashType{});
            EXPECT_EQ(img->getSize(), 0);
            EXPECT_EQ(img->getWidth(), 0);
            EXPECT_EQ(img->getHeight(), 0);
            EXPECT_EQ(img->getMimeType(), "");
        }

        {
            auto transaction{ session.createWriteTransaction() };

            TrackEmbeddedImage::pointer img{ TrackEmbeddedImage::find(session, image.getId()) };
            ASSERT_NE(img, TrackEmbeddedImage::pointer{});
            img.modify()->setHash(db::ImageHashType{ std::numeric_limits<std::uint64_t>::max() });
            img.modify()->setSize(1024 * 1024);
            img.modify()->setWidth(640);
            img.modify()->setHeight(480);
            img.modify()->setMimeType("image/jpeg");
        }

        {
            auto transaction{ session.createReadTransaction() };

            const TrackEmbeddedImage::pointer img{ TrackEmbeddedImage::find(session, image.getId()) };
            ASSERT_NE(img, TrackEmbeddedImage::pointer{});
            EXPECT_EQ(img->getHash(), db::ImageHashType{ std::numeric_limits<std::uint64_t>::max() });
            EXPECT_EQ(img->getSize(), 1024 * 1024);
            EXPECT_EQ(img->getWidth(), 640);
            EXPECT_EQ(img->getHeight(), 480);
            EXPECT_EQ(img->getMimeType(), "image/jpeg");
        }
    }

    TEST_F(DatabaseFixture, TrackEmbeddedImage_findByHash)
    {
        ScopedTrackEmbeddedImage image{ session };
        constexpr std::size_t size{ 1024 };
        constexpr db::ImageHashType hash{ 42 };
        {
            auto transaction{ session.createWriteTransaction() };

            TrackEmbeddedImage::pointer img{ TrackEmbeddedImage::find(session, image.getId()) };
            ASSERT_NE(img, TrackEmbeddedImage::pointer{});
            img.modify()->setHash(hash);
            img.modify()->setSize(size);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const TrackEmbeddedImage::pointer img{ TrackEmbeddedImage::find(session, size, hash) };
            ASSERT_NE(img, TrackEmbeddedImage::pointer{});
            EXPECT_EQ(image.getId(), img->getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const TrackEmbeddedImage::pointer img{ TrackEmbeddedImage::find(session, size + 1, hash) };
            EXPECT_EQ(img, TrackEmbeddedImage::pointer{});
        }
    }

    TEST_F(DatabaseFixture, TrackEmbeddedImage_findByParams)
    {
        ScopedTrackEmbeddedImage image{ session };
        ScopedTrack track{ session };
        ScopedRelease release{ session, "MyRelease" };
        ScopedMedium medium{ session, release.lockAndGet() };
        ScopedTrackEmbeddedImageLink link{ session, track.lockAndGet(), image.lockAndGet() };

        {
            auto transaction{ session.createReadTransaction() };

            TrackEmbeddedImage::FindParameters params;

            bool visited{};
            TrackEmbeddedImage::find(session, params, [&](const auto&) { visited = true; });
            EXPECT_TRUE(visited);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            link.get().modify()->setType(ImageType::FrontCover);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setRelease(release.get());
            track.get().modify()->setMedium(medium.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            TrackEmbeddedImage::FindParameters params;
            params.setRelease(release.getId());
            params.setSortMethod(TrackEmbeddedImageSortMethod::DiscNumberThenTrackNumberThenSizeDesc);

            bool visited{};
            TrackEmbeddedImage::find(session, params, [&](const auto&) { visited = true; });
            EXPECT_TRUE(visited);
        }

        {
            auto transaction{ session.createReadTransaction() };

            TrackEmbeddedImage::FindParameters params;
            params.setMedium(medium.getId());
            params.setSortMethod(TrackEmbeddedImageSortMethod::TrackNumberThenSizeDesc);

            bool visited{};
            TrackEmbeddedImage::find(session, params, [&](const auto&) { visited = true; });
            EXPECT_TRUE(visited);
        }

        {
            auto transaction{ session.createReadTransaction() };

            TrackEmbeddedImage::FindParameters params;
            params.setTrack(track.getId());

            bool visited{};
            TrackEmbeddedImage::find(session, params, [&](const auto&) { visited = true; });
            EXPECT_TRUE(visited);
        }
    }

    TEST_F(DatabaseFixture, TrackEmbeddedImage_findByParams_sorts)
    {
        ScopedTrackEmbeddedImage image1{ session };
        ScopedTrackEmbeddedImage image2{ session };
        ScopedTrackEmbeddedImage image3{ session };
        ScopedTrackEmbeddedImage image4{ session };
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedRelease release{ session, "MyRelease" };
        ScopedMedium medium1{ session, release.lockAndGet() };
        ScopedMedium medium2{ session, release.lockAndGet() };
        ScopedTrackEmbeddedImageLink link1{ session, track1.lockAndGet(), image1.lockAndGet() };
        ScopedTrackEmbeddedImageLink link2{ session, track1.lockAndGet(), image2.lockAndGet() };
        ScopedTrackEmbeddedImageLink link3{ session, track1.lockAndGet(), image3.lockAndGet() };
        ScopedTrackEmbeddedImageLink link4{ session, track2.lockAndGet(), image4.lockAndGet() };

        {
            auto transaction{ session.createWriteTransaction() };
            medium1.get().modify()->setPosition(1);
            medium2.get().modify()->setPosition(2);

            track1.get().modify()->setRelease(release.get());
            track1.get().modify()->setMedium(medium1.get());
            track1.get().modify()->setTrackNumber(2);

            link1.get().modify()->setType(ImageType::FrontCover);
            image1.get().modify()->setSize(750);
            link2.get().modify()->setType(ImageType::Media);
            image2.get().modify()->setSize(1000);
            link3.get().modify()->setType(ImageType::Media);
            image3.get().modify()->setSize(2000);

            track2.get().modify()->setRelease(release.get());
            track2.get().modify()->setMedium(medium2.get());
            track2.get().modify()->setTrackNumber(1);

            link4.get().modify()->setType(ImageType::Media);
            image4.get().modify()->setSize(1500);
        }

        {
            auto transaction{ session.createReadTransaction() };

            TrackEmbeddedImage::FindParameters params;
            params.setRelease(release.getId());
            params.setImageType(ImageType::Media);
            params.setSortMethod(TrackEmbeddedImageSortMethod::SizeDesc);

            std::vector<TrackEmbeddedImageId> visitedIds;
            TrackEmbeddedImage::find(session, params, [&](const TrackEmbeddedImage::pointer& image) { visitedIds.push_back(image->getId()); });
            ASSERT_EQ(visitedIds.size(), 3);
            EXPECT_EQ(visitedIds[0], image3.getId());
            EXPECT_EQ(visitedIds[1], image4.getId());
            EXPECT_EQ(visitedIds[2], image2.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            TrackEmbeddedImage::FindParameters params;
            params.setMedium(medium1.getId());
            params.setImageType(ImageType::Media);
            params.setSortMethod(TrackEmbeddedImageSortMethod::TrackNumberThenSizeDesc);

            std::vector<TrackEmbeddedImageId> visitedIds;
            TrackEmbeddedImage::find(session, params, [&](const TrackEmbeddedImage::pointer& image) { visitedIds.push_back(image->getId()); });
            ASSERT_EQ(visitedIds.size(), 2);
            EXPECT_EQ(visitedIds[0], image3.getId());
            EXPECT_EQ(visitedIds[1], image2.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            TrackEmbeddedImage::FindParameters params;
            params.setRelease(release.getId());
            params.setImageType(ImageType::Media);
            params.setSortMethod(TrackEmbeddedImageSortMethod::DiscNumberThenTrackNumberThenSizeDesc);

            std::vector<TrackEmbeddedImageId> visitedIds;
            TrackEmbeddedImage::find(session, params, [&](const TrackEmbeddedImage::pointer& image) { visitedIds.push_back(image->getId()); });
            ASSERT_EQ(visitedIds.size(), 3);
            EXPECT_EQ(visitedIds[0], image3.getId());
            EXPECT_EQ(visitedIds[1], image2.getId());
            EXPECT_EQ(visitedIds[2], image4.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            TrackEmbeddedImage::FindParameters params;
            params.setRelease(release.getId());
            params.setImageType(ImageType::BackCover);
            params.setSortMethod(TrackEmbeddedImageSortMethod::DiscNumberThenTrackNumberThenSizeDesc);

            std::vector<TrackEmbeddedImageId> visitedIds;
            TrackEmbeddedImage::find(session, params, [&](const TrackEmbeddedImage::pointer& image) { visitedIds.push_back(image->getId()); });
            ASSERT_EQ(visitedIds.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, TrackEmbeddedImage_findByParams_medium)
    {
        ScopedTrackEmbeddedImage image{ session };
        ScopedTrack track{ session };
        ScopedRelease release{ session, "MyRelease" };
        ScopedMedium medium{ session, release.lockAndGet() };
        ScopedMedium otherMedium{ session, release.lockAndGet() };
        ScopedTrackEmbeddedImageLink link{ session, track.lockAndGet(), image.lockAndGet() };

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setRelease(release.get());
            track.get().modify()->setMedium(medium.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            TrackEmbeddedImage::FindParameters params;
            params.setMedium(medium.getId());

            bool visited{};
            TrackEmbeddedImage::find(session, params, [&](const TrackEmbeddedImage::pointer&) { visited = true; });
            ASSERT_TRUE(visited);
        }

        {
            auto transaction{ session.createReadTransaction() };

            TrackEmbeddedImage::FindParameters params;
            params.setMedium(2);

            bool visited{};
            TrackEmbeddedImage::find(session, params, [&](const TrackEmbeddedImage::pointer&) { visited = true; });
            ASSERT_FALSE(visited);
        }
    }

    TEST_F(DatabaseFixture, Track_findByEmbeddedImage)
    {
        ScopedTrackEmbeddedImage image{ session };
        ScopedTrack track{ session };
        ScopedTrackEmbeddedImageLink link{ session, track.lockAndGet(), image.lockAndGet() };

        {
            auto transaction{ session.createReadTransaction() };

            Track::FindParameters params;
            params.setEmbeddedImage(image->getId());

            bool visited{};
            Track::find(session, params, [&](const auto&) { visited = true; });
            EXPECT_TRUE(visited);
        }
    }

    TEST_F(DatabaseFixture, TrackEmbeddedImage_findOrphans)
    {
        ScopedTrackEmbeddedImage image{ session };

        {
            auto transaction{ session.createReadTransaction() };

            auto orphans{ TrackEmbeddedImage::findOrphanIds(session, std::nullopt) };
            ASSERT_EQ(orphans.results.size(), 1);
            EXPECT_EQ(orphans.results[0], image.getId());
        }

        {
            ScopedTrack track{ session };
            ScopedTrackEmbeddedImageLink link{ session, track.lockAndGet(), image.lockAndGet() };

            {
                auto transaction{ session.createReadTransaction() };

                auto orphans{ TrackEmbeddedImage::findOrphanIds(session, std::nullopt) };
                ASSERT_EQ(orphans.results.size(), 0);
            }
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto orphans{ TrackEmbeddedImage::findOrphanIds(session, std::nullopt) };
            ASSERT_EQ(orphans.results.size(), 1);
            EXPECT_EQ(orphans.results[0], image.getId());
        }
    }

    TEST_F(DatabaseFixture, TrackEmbeddedImageLink)
    {
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(TrackEmbeddedImage::getCount(session), 0);
        }

        ScopedTrack track{ session };
        ScopedTrackEmbeddedImage image{ session };
        ScopedTrackEmbeddedImageLink imageLink{ session, track.lockAndGet(), image.lockAndGet() };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(TrackEmbeddedImageLink::getCount(session), 1);

            const TrackEmbeddedImageLink::pointer link{ TrackEmbeddedImageLink::find(session, imageLink.getId()) };
            ASSERT_NE(link, TrackEmbeddedImageLink::pointer{});
            EXPECT_EQ(link->getIndex(), 0);
            EXPECT_EQ(link->getType(), ImageType::Unknown);
            EXPECT_EQ(link->getDescription(), "");
            EXPECT_EQ(link->getTrack(), track.get());
            EXPECT_EQ(link->getImage(), image.get());
        }

        {
            auto transaction{ session.createWriteTransaction() };

            TrackEmbeddedImageLink::pointer link{ TrackEmbeddedImageLink::find(session, imageLink.getId()) };
            ASSERT_NE(link, TrackEmbeddedImage::pointer{});
            link.modify()->setIndex(2);
            link.modify()->setType(ImageType::FrontCover);
            link.modify()->setDescription("MyDesc");
        }

        {
            auto transaction{ session.createReadTransaction() };

            const TrackEmbeddedImageLink::pointer img{ TrackEmbeddedImageLink::find(session, imageLink.getId()) };
            ASSERT_NE(img, TrackEmbeddedImage::pointer{});
            EXPECT_EQ(img->getIndex(), 2);
            EXPECT_EQ(img->getType(), ImageType::FrontCover);
            EXPECT_EQ(img->getDescription(), "MyDesc");
        }

        {
            auto transaction{ session.createReadTransaction() };

            bool visited{};
            TrackEmbeddedImageLink::find(session, image->getId(), [&](const TrackEmbeddedImageLink::pointer& link) {
                EXPECT_EQ(link->getIndex(), 2);
                EXPECT_EQ(link->getType(), ImageType::FrontCover);
                EXPECT_EQ(link->getDescription(), "MyDesc");
                EXPECT_EQ(link->getTrack(), track.get());

                visited = true;
            });
            EXPECT_TRUE(visited);
        }
    }

    TEST_F(DatabaseFixture, TrackEmbeddedImage_TrackList)
    {
        ScopedTrackList trackList{ session, "MytrackList", TrackListType::PlayList };

        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedTrackEmbeddedImage image1{ session };
        ScopedTrackEmbeddedImageLink imageLink1{ session, track1.lockAndGet(), image1.lockAndGet() };
        ScopedTrackEmbeddedImage image2{ session };
        ScopedTrackEmbeddedImageLink imageLink2{ session, track2.lockAndGet(), image2.lockAndGet() };

        {
            auto transaction{ session.createWriteTransaction() };
            session.create<TrackListEntry>(track2.get(), trackList.get());
            session.create<TrackListEntry>(track1.get(), trackList.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            TrackEmbeddedImage::FindParameters params;
            params.setTrackList(trackList.getId());
            params.setSortMethod(TrackEmbeddedImageSortMethod::TrackListIndexAscThenSizeDesc);

            std::vector<TrackEmbeddedImageId> visitedIds;
            TrackEmbeddedImage::find(session, params, [&](const TrackEmbeddedImage::pointer& image) {
                visitedIds.push_back(image->getId());
            });
            ASSERT_EQ(visitedIds.size(), 2);
            EXPECT_EQ(visitedIds[0], image2.getId());
            EXPECT_EQ(visitedIds[1], image1.getId());
        }
    }
} // namespace lms::db::tests