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
#include <variant>

#include "database/Object.hpp"
#include "database/objects/ArtworkId.hpp"
#include "database/objects/ImageId.hpp"
#include "database/objects/TrackEmbeddedImageId.hpp"

namespace lms::db
{
    class ISession;
    class Image;
    class TrackEmbeddedImage;

    class Artwork : public Object<Artwork, ArtworkId>
    {
    public:
        static pointer create(ISession& session, const ObjectPtr<TrackEmbeddedImage>& trackEmbeddedImage);
        static pointer create(ISession& session, const ObjectPtr<Image>& image);

        // find
        static std::size_t getCount(ISession& session);
        static pointer find(ISession& session, ArtworkId id);
        static pointer find(ISession& session, TrackEmbeddedImageId id);
        static pointer find(ISession& session, ImageId id);

        // getters
        using UnderlyingId = std::variant<std::monostate, TrackEmbeddedImageId, ImageId>;
        UnderlyingId getUnderlyingId() const;
        std::filesystem::path getAbsoluteFilePath() const;
        ObjectPtr<Image> getImage() const;
        ImageId getImageId() const;

    protected:
        Artwork() = default;
    };
} // namespace lms::db
