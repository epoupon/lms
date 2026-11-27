/*
 * Copyright (C) 2026 Emeric Poupon
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

#include <Wt/Dbo/Dbo.h>
#include <Wt/Dbo/Field.h>
#include <Wt/Dbo/collection.h>

#include "database/objects/TrackEmbeddedImage.hpp"

#include "DboAccess.hpp"

namespace lms::db::dbo
{
    class TrackEmbeddedImageLink;

    class TrackEmbeddedImage final : public db::TrackEmbeddedImage, public Wt::Dbo::Dbo<TrackEmbeddedImage>
    {
    public:
        TrackEmbeddedImage() = default;
        using db::TrackEmbeddedImage::TrackEmbeddedImage;

        void syncIds()
        {
            _id = id();
        }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _hash, "hash");
            Wt::Dbo::field(a, _size, "size");
            Wt::Dbo::field(a, _width, "width");
            Wt::Dbo::field(a, _height, "height");
            Wt::Dbo::field(a, _format, "format");

            Wt::Dbo::hasMany(a, _links, Wt::Dbo::ManyToOne, "track_embedded_image");
        }

        Wt::Dbo::collection<Wt::Dbo::ptr<TrackEmbeddedImageLink>> _links;
    };
} // namespace lms::db::dbo

namespace lms::db
{
    template<>
    struct DboOf<TrackEmbeddedImage>
    {
        using type = dbo::TrackEmbeddedImage;
    };
} // namespace lms::db
