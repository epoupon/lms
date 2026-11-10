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
#include <optional>
#include <span>
#include <variant>

#include "database/objects/ArtworkId.hpp"
#include "database/objects/ImageId.hpp"
#include "database/objects/TrackEmbeddedImageId.hpp"
#include "database/objects/detail/Object.hpp"

namespace lms::db
{
    class ISession;

    class Artwork : public detail::Object<Artwork, ArtworkId>
    {
    public:
        static Artwork create(ISession& session, TrackEmbeddedImageId trackEmbeddedImage);
        static Artwork create(ISession& session, ImageId image);
        static void destroy(ISession& session, std::span<const ArtworkId> ids);

        // find
        static std::size_t getCount(ISession& session);
        static std::optional<Artwork> find(ISession& session, ArtworkId id);
        static std::optional<Artwork> find(ISession& session, TrackEmbeddedImageId id);
        static std::optional<Artwork> find(ISession& session, ImageId id);
        static std::filesystem::path getAbsoluteFilePath(ISession& session, ArtworkId id);

        // getters
        using UnderlyingId = std::variant<std::monostate, TrackEmbeddedImageId, ImageId>;
        UnderlyingId getUnderlyingId() const;
        ImageId getImageId() const { return _imageId; }

    protected:
        Artwork() = default;

        TrackEmbeddedImageId _trackEmbeddedImageId;
        ImageId _imageId;
    };
} // namespace lms::db
