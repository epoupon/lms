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

#include <optional>
#include <span>

#include "core/media/ImageFormat.hpp"
#include "core/media/ImageType.hpp"

#include "database/Types.hpp"
#include "database/objects/MediumId.hpp"
#include "database/objects/ReleaseId.hpp"
#include "database/objects/TrackEmbeddedImageId.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/TrackListId.hpp"
#include "database/objects/Types.hpp"
#include "database/objects/detail/Object.hpp"
#include "database/objects/detail/Types.hpp"

namespace lms::db
{
    class ISession;

    class TrackEmbeddedImage : public detail::Object<TrackEmbeddedImage, TrackEmbeddedImageId>
    {
    public:
        static TrackEmbeddedImage create(ISession& session);
        static void destroy(ISession& session, std::span<const TrackEmbeddedImageId> ids);

        struct FindParameters
        {
            std::optional<Range> range;
            TrackId track;
            ReleaseId release;
            MediumId medium;
            TrackListId trackList;
            std::optional<core::media::ImageType> imageType;
            TrackEmbeddedImageSortMethod sortMethod{ TrackEmbeddedImageSortMethod::None };

            FindParameters& setRange(std::optional<Range> _range)
            {
                range = _range;
                return *this;
            }
            FindParameters& setTrack(TrackId _track)
            {
                track = _track;
                return *this;
            }
            FindParameters& setRelease(ReleaseId _release)
            {
                release = _release;
                return *this;
            }
            FindParameters& setMedium(MediumId _medium)
            {
                medium = _medium;
                return *this;
            }
            FindParameters& setTrackList(TrackListId _trackList)
            {
                trackList = _trackList;
                return *this;
            }
            FindParameters& setImageType(std::optional<core::media::ImageType> _imageType)
            {
                imageType = _imageType;
                return *this;
            }
            FindParameters& setSortMethod(TrackEmbeddedImageSortMethod _sortMethod)
            {
                sortMethod = _sortMethod;
                return *this;
            }
        };

        // find
        static std::size_t getCount(ISession& session);
        static std::optional<TrackEmbeddedImage> find(ISession& session, TrackEmbeddedImageId id);
        static void find(ISession& session, const FindParameters& params, const std::function<void(const TrackEmbeddedImage&)>& func);
        static std::optional<TrackEmbeddedImage> find(ISession& session, std::size_t size, ImageHashType hash);
        static std::vector<TrackEmbeddedImageId> findOrphanIds(ISession& session, std::optional<Range> range);

        static void update(ISession& session, const TrackEmbeddedImage& trackEmbeddedImage);

        // getters
        ImageHashType getHash() const { return _hash; }
        std::size_t getSize() const { return _size; }
        std::size_t getWidth() const { return _width; }
        std::size_t getHeight() const { return _height; }
        std::optional<core::media::ImageFormat> getFormat() const;

        // setters
        void setHash(ImageHashType hash) { _hash = hash; }
        void setSize(std::size_t size) { _size = static_cast<int>(size); }
        void setWidth(std::size_t width) { _width = static_cast<int>(width); }
        void setHeight(std::size_t height) { _height = static_cast<int>(height); }
        void setFormat(core::media::ImageFormat format);

    protected:
        TrackEmbeddedImage() = default;
        ImageHashType _hash;
        int _size{};
        int _width{};
        int _height{};
        detail::ImageFormat _format{ detail::ImageFormat::Unknown };
    };
} // namespace lms::db
