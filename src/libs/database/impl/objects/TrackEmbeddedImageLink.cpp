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

#include "database/objects/TrackEmbeddedImageLink.hpp"

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "database/Session.hpp"
#include "database/objects/Genre.hpp"
#include "database/objects/Grouping.hpp"
#include "database/objects/Language.hpp"
#include "database/objects/Mood.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackEmbeddedImage.hpp"

#include "Utils.hpp"
#include "detail/Types.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/ImageHashTypeTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::TrackEmbeddedImageLink)

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

    void TrackEmbeddedImageLink::find(Session& session, TrackEmbeddedImageId trackEmbeddedImageId, std::function<void(const pointer&)> visitor)
    {
        auto query{ session.getDboSession()->find<TrackEmbeddedImageLink>() };
        query.where("track_embedded_image_id = ?").bind(trackEmbeddedImageId);

        return utils::forEachQueryResult(query, visitor);
    }

    ObjectPtr<Track> TrackEmbeddedImageLink::getTrack() const
    {
        return _track;
    }

    ObjectPtr<TrackEmbeddedImage> TrackEmbeddedImageLink::getImage() const
    {
        return _image;
    }

    core::media::ImageType TrackEmbeddedImageLink::getType() const
    {
        return detail::getMediaImageType(_type);
    }

    void TrackEmbeddedImageLink::setType(core::media::ImageType type)
    {
        _type = detail::getDbImageType(type);
    }
} // namespace lms::db
