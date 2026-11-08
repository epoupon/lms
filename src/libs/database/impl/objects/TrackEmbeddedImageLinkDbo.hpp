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

#include "database/objects/TrackEmbeddedImageLink.hpp"

#include "ObjectPtrImpl.hpp"

namespace lms::db::dbo
{
    class Track;
    class TrackEmbeddedImage;

    class TrackEmbeddedImageLink final : public db::TrackEmbeddedImageLink, public Wt::Dbo::Dbo<TrackEmbeddedImageLink>
    {
    public:
        TrackEmbeddedImageLink() = default;
        using db::TrackEmbeddedImageLink::TrackEmbeddedImageLink;

        TrackEmbeddedImageLink(const ObjectPtr<db::Track>& track, const ObjectPtr<db::TrackEmbeddedImage>& image);

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _index, "index");
            Wt::Dbo::field(a, _type, "type");
            Wt::Dbo::field(a, _description, "description");

            Wt::Dbo::belongsTo(a, _track, "track", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::belongsTo(a, _image, "track_embedded_image", Wt::Dbo::OnDeleteCascade);
        }

        Wt::Dbo::ptr<Track> _track;
        Wt::Dbo::ptr<TrackEmbeddedImage> _image;
    };
} // namespace lms::db::dbo

namespace lms::db
{
    template<>
    struct DboOf<TrackEmbeddedImageLink>
    {
        using type = dbo::TrackEmbeddedImageLink;
    };
} // namespace lms::db
