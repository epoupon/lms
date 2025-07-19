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

#pragma once

#include <filesystem>

#include <Wt/Dbo/Field.h>
#include <Wt/WDateTime.h>

#include "database/Object.hpp"
#include "database/objects/ArtworkId.hpp"
#include "database/objects/ImageId.hpp"
#include "database/objects/TrackEmbeddedImageId.hpp"

namespace lms::db
{
    class Image;
    class Session;
    class TrackEmbeddedImage;

    class Artwork final : public Object<Artwork, ArtworkId>
    {
    public:
        Artwork() = default;

        // find
        static std::size_t getCount(Session& session);
        static pointer find(Session& session, ArtworkId id);
        static pointer find(Session& session, TrackEmbeddedImageId id);
        static pointer find(Session& session, ImageId id);

        // getters
        TrackEmbeddedImageId getTrackEmbeddedImageId() const { return _trackEmbeddedImage.id(); }
        ImageId getImageId() const { return _image.id(); }
        Wt::WDateTime getLastWrittenTime() const;
        std::filesystem::path getAbsoluteFilePath() const;

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::belongsTo(a, _trackEmbeddedImage, "track_embedded_image", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::belongsTo(a, _image, "image", Wt::Dbo::OnDeleteCascade);
        }

    private:
        friend class Session;
        Artwork(ObjectPtr<TrackEmbeddedImage> trackEmbeddedImage);
        Artwork(ObjectPtr<Image> image);
        static pointer create(Session& session, ObjectPtr<TrackEmbeddedImage> trackEmbeddedImage);
        static pointer create(Session& session, ObjectPtr<Image> image);

        Wt::Dbo::ptr<TrackEmbeddedImage> _trackEmbeddedImage;
        Wt::Dbo::ptr<Image> _image;
    };
} // namespace lms::db
