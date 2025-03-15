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

#include "database/TrackEmbeddedImageLink.hpp"

#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/TrackEmbeddedImage.hpp"
#include "database/Types.hpp"

#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

namespace lms::db
{
    TrackEmbeddedImageLink::TrackEmbeddedImageLink(ObjectPtr<Track> track, ObjectPtr<TrackEmbeddedImage> image)
        : _track{ getDboPtr(track) }
        , _image{ getDboPtr(image) }
    {
    }

    TrackEmbeddedImageLink::pointer TrackEmbeddedImageLink::create(Session& session, ObjectPtr<Track> track, ObjectPtr<TrackEmbeddedImage> image)
    {
        return session.getDboSession()->add(std::unique_ptr<TrackEmbeddedImageLink>{ new TrackEmbeddedImageLink{ track, image } });
    }

    std::size_t TrackEmbeddedImageLink::getCount(Session& session)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM track_embedded_image_link"));
    }

    TrackEmbeddedImageLink::pointer TrackEmbeddedImageLink::find(Session& session, TrackEmbeddedImageLinkId id)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->find<TrackEmbeddedImageLink>().where("id = ?").bind(id));
    }

    ObjectPtr<Track> TrackEmbeddedImageLink::getTrack() const
    {
        return _track;
    }

    ObjectPtr<TrackEmbeddedImage> TrackEmbeddedImageLink::getImage() const
    {
        return _image;
    }
} // namespace lms::db
