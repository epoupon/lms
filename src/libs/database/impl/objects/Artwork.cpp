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

#include "database/objects/Artwork.hpp"

#include <Wt/Dbo/Impl.h>

#include "ArtworkDbo.hpp"
#include "DboAccess.hpp"
#include "ImageDbo.hpp"
#include "Session.hpp"
#include "TrackEmbeddedImageDbo.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/PathTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::Artwork)

namespace lms::db
{
    dbo::Artwork::Artwork(Wt::Dbo::ptr<TrackEmbeddedImage> trackEmbeddedImage)
        : _trackEmbeddedImage{ std::move(trackEmbeddedImage) }
    {
    }

    dbo::Artwork::Artwork(Wt::Dbo::ptr<Image> image)
        : _image{ std::move(image) }
    {
    }

    Artwork Artwork::create(ISession& session, TrackEmbeddedImageId trackEmbeddedImage)
    {
        return utils::addObject(session, std::unique_ptr<dbo::Artwork>{ new dbo::Artwork{ getDboSession(session)->loadLazy<dbo::TrackEmbeddedImage>(trackEmbeddedImage.getValue()) } });
    }

    void Artwork::destroy(ISession& session, std::span<const ArtworkId> ids)
    {
        utils::destroyObjects<dbo::Artwork>(session, ids);
    }

    Artwork Artwork::create(ISession& session, ImageId image)
    {
        return utils::addObject(session, std::unique_ptr<dbo::Artwork>{ new dbo::Artwork{ getDboSession(session)->loadLazy<dbo::Image>(image.getValue()) } });
    }

    std::size_t Artwork::getCount(ISession& session)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<long>("SELECT COUNT(*) FROM artwork"));
    }

    std::optional<Artwork> Artwork::find(ISession& session, ArtworkId id)
    {
        checkReadTransaction(session);

        if (!id.isValid())
            return std::nullopt;

        return utils::fetchQuerySingleObject(getDboSession(session)->query<Wt::Dbo::ptr<dbo::Artwork>>("SELECT a FROM artwork a").where("a.id = ?").bind(id));
    }

    std::optional<Artwork> Artwork::find(ISession& session, TrackEmbeddedImageId id)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleObject(getDboSession(session)->query<Wt::Dbo::ptr<dbo::Artwork>>("SELECT a FROM artwork a").where("a.track_embedded_image_id = ?").bind(id));
    }

    std::optional<Artwork> Artwork::find(ISession& session, ImageId id)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleObject(getDboSession(session)->query<Wt::Dbo::ptr<dbo::Artwork>>("SELECT a FROM artwork a").where("a.image_id = ?").bind(id));
    }

    Artwork::UnderlyingId Artwork::getUnderlyingId() const
    {
        Artwork::UnderlyingId res;

        if (_trackEmbeddedImageId.isValid())
            res = _trackEmbeddedImageId;
        else if (_imageId.isValid())
            res = _imageId;

        return res;
    }

    std::filesystem::path Artwork::getAbsoluteFilePath(ISession& session, ArtworkId id)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<std::filesystem::path>("SELECT COALESCE(image.absolute_file_path, track.absolute_file_path) AS absolute_file_path FROM artwork") };
        query.leftJoin("image ON artwork.image_id = image.id");
        query.leftJoin("track_embedded_image ON artwork.track_embedded_image_id = track_embedded_image.id");
        query.leftJoin("track_embedded_image_link ON track_embedded_image.id = track_embedded_image_link.track_embedded_image_id");
        query.leftJoin("track ON track.id = track_embedded_image_link.track_id");
        query.limit(1); // there may be several tracks matching this artwork...
        query.where("artwork.id = ?").bind(id);

        return utils::fetchQuerySingleResult(query);
    }

} // namespace lms::db
