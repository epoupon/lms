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
#include <string>
#include <string_view>

#include <Wt/Dbo/Field.h>
#include <Wt/Dbo/collection.h>

#include "database/Object.hpp"
#include "database/Types.hpp"
#include "database/objects/MediumId.hpp"
#include "database/objects/ReleaseId.hpp"
#include "database/objects/TrackEmbeddedImageId.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/TrackListId.hpp"

namespace lms::db
{
    class TrackEmbeddedImageLink;
    class Session;
    class Track;

    class TrackEmbeddedImage final : public Object<TrackEmbeddedImage, TrackEmbeddedImageId>
    {
    public:
        TrackEmbeddedImage() = default;

        struct FindParameters
        {
            std::optional<Range> range;
            TrackId track;
            ReleaseId release;
            MediumId medium;
            TrackListId trackList;
            std::optional<ImageType> imageType;
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
            FindParameters& setImageType(std::optional<ImageType> _imageType)
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
        static std::size_t getCount(Session& session);
        static pointer find(Session& session, TrackEmbeddedImageId id);
        static void find(Session& session, const FindParameters& params, const std::function<void(const pointer&)>& func);
        static pointer find(Session& session, std::size_t size, ImageHashType hash);
        static RangeResults<TrackEmbeddedImageId> findOrphanIds(Session& session, std::optional<Range> range);

        // getters
        ImageHashType getHash() const { return _hash; }
        std::size_t getSize() const { return _size; }
        std::size_t getWidth() const { return _width; }
        std::size_t getHeight() const { return _height; }
        std::string_view getMimeType() const { return _mimeType; }

        // setters
        void setHash(ImageHashType hash) { _hash = hash; }
        void setSize(std::size_t size) { _size = static_cast<int>(size); }
        void setWidth(std::size_t width) { _width = static_cast<int>(width); }
        void setHeight(std::size_t height) { _height = static_cast<int>(height); }
        void setMimeType(std::string_view mimeType) { _mimeType = mimeType; }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _hash, "hash");
            Wt::Dbo::field(a, _size, "size");
            Wt::Dbo::field(a, _width, "width");
            Wt::Dbo::field(a, _height, "height");
            Wt::Dbo::field(a, _mimeType, "mime_type");

            Wt::Dbo::hasMany(a, _links, Wt::Dbo::ManyToOne, "track_embedded_image");
        }

    private:
        friend class Session;

        static pointer create(Session& session);

        ImageHashType _hash;
        int _size{};
        int _width{};
        int _height{};
        std::string _mimeType;

        Wt::Dbo::collection<Wt::Dbo::ptr<TrackEmbeddedImageLink>> _links;
    };
} // namespace lms::db
