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

#pragma once

#include <string>
#include <string_view>

#include "core/media/ImageType.hpp"

#include "database/Object.hpp"
#include "database/objects/TrackEmbeddedImageId.hpp"
#include "database/objects/TrackEmbeddedImageLinkId.hpp"
#include "database/objects/Types.hpp"
#include "database/objects/detail/Types.hpp"

namespace lms::db
{
    class ISession;
    class TrackEmbeddedImage;
    class Track;

    class TrackEmbeddedImageLink : public Object<TrackEmbeddedImageLink, TrackEmbeddedImageLinkId>
    {
    public:
        static pointer create(ISession& session, const ObjectPtr<Track>& track, const ObjectPtr<TrackEmbeddedImage>& image);

        // find
        static std::size_t getCount(ISession& session);
        static pointer find(ISession& session, TrackEmbeddedImageLinkId id);
        static void find(ISession& session, TrackEmbeddedImageId trackEmbeddedImageId, std::function<void(const pointer&)> visitor);

        // getters
        ObjectPtr<Track> getTrack() const;
        ObjectPtr<TrackEmbeddedImage> getImage() const;
        std::size_t getIndex() const { return _index; }
        core::media::ImageType getType() const;
        std::string_view getDescription() const { return _description; }

        // setters
        void setIndex(std::size_t index) { _index = static_cast<int>(index); }
        void setType(core::media::ImageType type);
        void setDescription(std::string_view description) { _description = description; }

    protected:
        TrackEmbeddedImageLink() = default;

        int _index{}; // index within the track
        detail::ImageType _type{ detail::ImageType::Unknown };
        std::string _description;
    };
} // namespace lms::db
