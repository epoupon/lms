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

            const std::optional<TrackEmbeddedImage> img{ TrackEmbeddedImage::find(session, image.getId()) };
            ASSERT_TRUE(img);
            EXPECT_EQ(img->getHash(), db::ImageHashType{});
            EXPECT_EQ(img->getSize(), 0);
            EXPECT_EQ(img->getWidth(), 0);
            EXPECT_EQ(img->getHeight(), 0);
            EXPECT_EQ(img->getFormat(), std::nullopt);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<TrackEmbeddedImage> img{ TrackEmbeddedImage::find(session, image.getId()) };
            ASSERT_TRUE(img);
            img->setHash(db::ImageHashType{ std::numeric_limits<std::uint64_t>::max() });
            img->setSize(1024 * 1024);
            img->setWidth(640);
            img->setHeight(480);
            img->setFormat(core::media::ImageFormat::JPEG);
            TrackEmbeddedImage::update(session, *img);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const std::optional<TrackEmbeddedImage> img{ TrackEmbeddedImage::find(session, image.getId()) };
            ASSERT_TRUE(img);
            EXPECT_EQ(img->getHash(), db::ImageHashType{ std::numeric_limits<std::uint64_t>::max() });
            EXPECT_EQ(img->getSize(), 1024 * 1024);
            EXPECT_EQ(img->getWidth(), 640);
            EXPECT_EQ(img->getHeight(), 480);
            EXPECT_EQ(img->getFormat(), core::media::ImageFormat::JPEG);
        }
    }

    TEST_F(DatabaseFixture, TrackEmbeddedImage_findByHash)
    {
        ScopedTrackEmbeddedImage image{ session };
        constexpr std::size_t size{ 1024 };
        constexpr db::ImageHashType hash{ 42 };
        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<TrackEmbeddedImage> img{ TrackEmbeddedImage::find(session, image.getId()) };
            ASSERT_TRUE(img);
            img->setHash(hash);
            img->setSize(size);
            TrackEmbeddedImage::update(session, *img);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const std::optional<TrackEmbeddedImage> img{ TrackEmbeddedImage::find(session, size, hash) };
            ASSERT_TRUE(img);
            EXPECT_EQ(image.getId(), img->getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const std::optional<TrackEmbeddedImage> img{ TrackEmbeddedImage::find(session, size + 1, hash) };
            EXPECT_FALSE(img);
        }
    }

    TEST_F(DatabaseFixture, TrackEmbeddedImage_findByParams)
    {
        ScopedTrackEmbeddedImage image{ session };
        ScopedTrack track{ session };
        ScopedRelease release{ session, "MyRelease" };
        ScopedMedium medium{ session, release.getId() };
        ScopedTrackEmbeddedImageLink link{ session, track.getId(), image.getId() };

        {
            auto transaction{ session.createReadTransaction() };

            TrackEmbeddedImage::FindParameters params;

            bool visited{};
            TrackEmbeddedImage::find(session, params, [&](const auto&) { visited = true; });
            EXPECT_TRUE(visited);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<TrackEmbeddedImageLink> updatedLink{ link.get() };
            updatedLink->setType(core::media::ImageType::FrontCover);
            TrackEmbeddedImageLink::update(session, *updatedLink);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Track> updatedTrack{ track.get() };
            updatedTrack->setRelease(release.getId());
            updatedTrack->setMedium(medium.getId());
            Track::update(session, *updatedTrack);
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

        {
            auto transaction{ session.createReadTransaction() };

            TrackEmbeddedImage::FindParameters params;
            params.setTrack(track.getId());
            params.setImageType(core::media::ImageType::FrontCover);

            bool visited{};
            TrackEmbeddedImage::find(session, params, [&](const auto&) { visited = true; });
            EXPECT_TRUE(visited);
        }

        {
            auto transaction{ session.createReadTransaction() };

            TrackEmbeddedImage::FindParameters params;
            params.setTrack(track.getId());
            params.setImageType(core::media::ImageType::BackCover);

            bool visited{};
            TrackEmbeddedImage::find(session, params, [&](const auto&) { visited = true; });
            EXPECT_FALSE(visited);
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
        ScopedMedium medium1{ session, release.getId() };
        ScopedMedium medium2{ session, release.getId() };
        ScopedTrackEmbeddedImageLink link1{ session, track1.getId(), image1.getId() };
        ScopedTrackEmbeddedImageLink link2{ session, track1.getId(), image2.getId() };
        ScopedTrackEmbeddedImageLink link3{ session, track1.getId(), image3.getId() };
        ScopedTrackEmbeddedImageLink link4{ session, track2.getId(), image4.getId() };

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Medium> updatedMedium1{ medium1.get() };
            updatedMedium1->setPosition(1);
            Medium::update(session, *updatedMedium1);

            std::optional<Medium> updatedMedium2{ medium2.get() };
            updatedMedium2->setPosition(2);
            Medium::update(session, *updatedMedium2);

            std::optional<Track> updatedTrack1{ track1.get() };
            updatedTrack1->setRelease(release.getId());
            updatedTrack1->setMedium(medium1.getId());
            updatedTrack1->setTrackNumber(2);
            Track::update(session, *updatedTrack1);

            std::optional<TrackEmbeddedImageLink> updatedLink1{ link1.get() };
            updatedLink1->setType(core::media::ImageType::FrontCover);
            TrackEmbeddedImageLink::update(session, *updatedLink1);
            std::optional<TrackEmbeddedImage> updatedImage1{ image1.get() };
            updatedImage1->setSize(750);
            TrackEmbeddedImage::update(session, *updatedImage1);
            std::optional<TrackEmbeddedImageLink> updatedLink2{ link2.get() };
            updatedLink2->setType(core::media::ImageType::Media);
            TrackEmbeddedImageLink::update(session, *updatedLink2);
            std::optional<TrackEmbeddedImage> updatedImage2{ image2.get() };
            updatedImage2->setSize(1000);
            TrackEmbeddedImage::update(session, *updatedImage2);
            std::optional<TrackEmbeddedImageLink> updatedLink3{ link3.get() };
            updatedLink3->setType(core::media::ImageType::Media);
            TrackEmbeddedImageLink::update(session, *updatedLink3);
            std::optional<TrackEmbeddedImage> updatedImage3{ image3.get() };
            updatedImage3->setSize(2000);
            TrackEmbeddedImage::update(session, *updatedImage3);

            std::optional<Track> updatedTrack2{ track2.get() };
            updatedTrack2->setRelease(release.getId());
            updatedTrack2->setMedium(medium2.getId());
            updatedTrack2->setTrackNumber(1);
            Track::update(session, *updatedTrack2);

            std::optional<TrackEmbeddedImageLink> updatedLink4{ link4.get() };
            updatedLink4->setType(core::media::ImageType::Media);
            TrackEmbeddedImageLink::update(session, *updatedLink4);
            std::optional<TrackEmbeddedImage> updatedImage4{ image4.get() };
            updatedImage4->setSize(1500);
            TrackEmbeddedImage::update(session, *updatedImage4);
        }

        {
            auto transaction{ session.createReadTransaction() };

            TrackEmbeddedImage::FindParameters params;
            params.setRelease(release.getId());
            params.setImageType(core::media::ImageType::Media);
            params.setSortMethod(TrackEmbeddedImageSortMethod::SizeDesc);

            std::vector<TrackEmbeddedImageId> visitedIds;
            TrackEmbeddedImage::find(session, params, [&](const TrackEmbeddedImage& image) { visitedIds.push_back(image.getId()); });
            ASSERT_EQ(visitedIds.size(), 3);
            EXPECT_EQ(visitedIds[0], image3.getId());
            EXPECT_EQ(visitedIds[1], image4.getId());
            EXPECT_EQ(visitedIds[2], image2.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            TrackEmbeddedImage::FindParameters params;
            params.setMedium(medium1.getId());
            params.setImageType(core::media::ImageType::Media);
            params.setSortMethod(TrackEmbeddedImageSortMethod::TrackNumberThenSizeDesc);

            std::vector<TrackEmbeddedImageId> visitedIds;
            TrackEmbeddedImage::find(session, params, [&](const TrackEmbeddedImage& image) { visitedIds.push_back(image.getId()); });
            ASSERT_EQ(visitedIds.size(), 2);
            EXPECT_EQ(visitedIds[0], image3.getId());
            EXPECT_EQ(visitedIds[1], image2.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            TrackEmbeddedImage::FindParameters params;
            params.setRelease(release.getId());
            params.setImageType(core::media::ImageType::Media);
            params.setSortMethod(TrackEmbeddedImageSortMethod::DiscNumberThenTrackNumberThenSizeDesc);

            std::vector<TrackEmbeddedImageId> visitedIds;
            TrackEmbeddedImage::find(session, params, [&](const TrackEmbeddedImage& image) { visitedIds.push_back(image.getId()); });
            ASSERT_EQ(visitedIds.size(), 3);
            EXPECT_EQ(visitedIds[0], image3.getId());
            EXPECT_EQ(visitedIds[1], image2.getId());
            EXPECT_EQ(visitedIds[2], image4.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            TrackEmbeddedImage::FindParameters params;
            params.setRelease(release.getId());
            params.setImageType(core::media::ImageType::BackCover);
            params.setSortMethod(TrackEmbeddedImageSortMethod::DiscNumberThenTrackNumberThenSizeDesc);

            std::vector<TrackEmbeddedImageId> visitedIds;
            TrackEmbeddedImage::find(session, params, [&](const TrackEmbeddedImage& image) { visitedIds.push_back(image.getId()); });
            ASSERT_EQ(visitedIds.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, TrackEmbeddedImage_findByParams_medium)
    {
        ScopedTrackEmbeddedImage image{ session };
        ScopedTrack track{ session };
        ScopedRelease release{ session, "MyRelease" };
        ScopedMedium medium{ session, release.getId() };
        ScopedMedium otherMedium{ session, release.getId() };
        ScopedTrackEmbeddedImageLink link{ session, track.getId(), image.getId() };

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Track> updatedTrack{ track.get() };
            updatedTrack->setRelease(release.getId());
            updatedTrack->setMedium(medium.getId());
            Track::update(session, *updatedTrack);
        }

        {
            auto transaction{ session.createReadTransaction() };

            TrackEmbeddedImage::FindParameters params;
            params.setMedium(medium.getId());

            bool visited{};
            TrackEmbeddedImage::find(session, params, [&](const TrackEmbeddedImage&) { visited = true; });
            ASSERT_TRUE(visited);
        }

        {
            auto transaction{ session.createReadTransaction() };

            TrackEmbeddedImage::FindParameters params;
            params.setMedium(2);

            bool visited{};
            TrackEmbeddedImage::find(session, params, [&](const TrackEmbeddedImage&) { visited = true; });
            ASSERT_FALSE(visited);
        }
    }

    TEST_F(DatabaseFixture, TrackEmbeddedImage_findOrphans)
    {
        ScopedTrackEmbeddedImage image{ session };

        {
            auto transaction{ session.createReadTransaction() };

            auto orphans{ TrackEmbeddedImage::findOrphanIds(session, std::nullopt) };
            ASSERT_EQ(orphans.size(), 1);
            EXPECT_EQ(orphans[0], image.getId());
        }

        {
            ScopedTrack track{ session };
            ScopedTrackEmbeddedImageLink link{ session, track.getId(), image.getId() };

            {
                auto transaction{ session.createReadTransaction() };

                auto orphans{ TrackEmbeddedImage::findOrphanIds(session, std::nullopt) };
                ASSERT_EQ(orphans.size(), 0);
            }
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto orphans{ TrackEmbeddedImage::findOrphanIds(session, std::nullopt) };
            ASSERT_EQ(orphans.size(), 1);
            EXPECT_EQ(orphans[0], image.getId());
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
        ScopedTrackEmbeddedImageLink imageLink{ session, track.getId(), image.getId() };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(TrackEmbeddedImageLink::getCount(session), 1);

            const std::optional<TrackEmbeddedImageLink> link{ TrackEmbeddedImageLink::find(session, imageLink.getId()) };
            ASSERT_TRUE(link);
            EXPECT_EQ(link->getIndex(), 0);
            EXPECT_EQ(link->getType(), core::media::ImageType::Unknown);
            EXPECT_EQ(link->getDescription(), "");
            EXPECT_EQ(link->getTrackId(), track.getId());
            EXPECT_EQ(link->getImageId(), image.getId());
        }

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<TrackEmbeddedImageLink> link{ TrackEmbeddedImageLink::find(session, imageLink.getId()) };
            ASSERT_TRUE(link);
            link->setIndex(2);
            link->setType(core::media::ImageType::FrontCover);
            link->setDescription("MyDesc");
            TrackEmbeddedImageLink::update(session, *link);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const std::optional<TrackEmbeddedImageLink> img{ TrackEmbeddedImageLink::find(session, imageLink.getId()) };
            ASSERT_TRUE(img);
            EXPECT_EQ(img->getIndex(), 2);
            EXPECT_EQ(img->getType(), core::media::ImageType::FrontCover);
            EXPECT_EQ(img->getDescription(), "MyDesc");
        }

        {
            auto transaction{ session.createReadTransaction() };

            bool visited{};
            TrackEmbeddedImageLink::find(session, image->getId(), [&](const TrackEmbeddedImageLink& link) {
                EXPECT_EQ(link.getIndex(), 2);
                EXPECT_EQ(link.getType(), core::media::ImageType::FrontCover);
                EXPECT_EQ(link.getDescription(), "MyDesc");
                EXPECT_EQ(link.getTrackId(), track.getId());

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
        ScopedTrackEmbeddedImageLink imageLink1{ session, track1.getId(), image1.getId() };
        ScopedTrackEmbeddedImage image2{ session };
        ScopedTrackEmbeddedImageLink imageLink2{ session, track2.getId(), image2.getId() };

        {
            auto transaction{ session.createWriteTransaction() };
            TrackListEntry::create(session, track2.getId(), trackList.getId());
            TrackListEntry::create(session, track1.getId(), trackList.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            TrackEmbeddedImage::FindParameters params;
            params.setTrackList(trackList.getId());
            params.setSortMethod(TrackEmbeddedImageSortMethod::TrackListIndexAscThenSizeDesc);

            std::vector<TrackEmbeddedImageId> visitedIds;
            TrackEmbeddedImage::find(session, params, [&](const TrackEmbeddedImage& image) {
                visitedIds.push_back(image.getId());
            });
            ASSERT_EQ(visitedIds.size(), 2);
            EXPECT_EQ(visitedIds[0], image2.getId());
            EXPECT_EQ(visitedIds[1], image1.getId());
        }
    }
} // namespace lms::db::tests