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

#include "database/objects/Genre.hpp"
#include "database/objects/Grouping.hpp"
#include "database/objects/Language.hpp"
#include "database/objects/Mood.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackEmbeddedImage.hpp"

#include "Session.hpp"
#include "Utils.hpp"
#include "detail/Types.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/ImageHashTypeTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::TrackEmbeddedImageLink)

namespace lms::db
{
    TrackEmbeddedImageLink::TrackEmbeddedImageLink(const ObjectPtr<Track>& track, const ObjectPtr<TrackEmbeddedImage>& image)
        : _track{ getDboPtr(track) }
        , _image{ getDboPtr(image) }
    {
    }

    TrackEmbeddedImageLink::pointer TrackEmbeddedImageLink::create(ISession& session, const ObjectPtr<Track>& track, const ObjectPtr<TrackEmbeddedImage>& image)
    {
        return utils::addObject(session, std::unique_ptr<TrackEmbeddedImageLink>{ new TrackEmbeddedImageLink{ track, image } });
    }

    std::size_t TrackEmbeddedImageLink::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM track_embedded_image_link"));
    }

    TrackEmbeddedImageLink::pointer TrackEmbeddedImageLink::find(ISession& session, TrackEmbeddedImageLinkId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->find<TrackEmbeddedImageLink>().where("id = ?").bind(id));
    }

    void TrackEmbeddedImageLink::find(ISession& session, TrackEmbeddedImageId trackEmbeddedImageId, std::function<void(const pointer&)> visitor)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->find<TrackEmbeddedImageLink>() };
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
