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
#include <Wt/Dbo/WtSqlTraits.h>

#include "database/Session.hpp"
#include "database/objects/Image.hpp"
#include "database/objects/TrackEmbeddedImage.hpp"

#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/PathTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::Artwork)

namespace lms::db
{
    Artwork::Artwork(ObjectPtr<TrackEmbeddedImage> trackEmbeddedImage)
        : _trackEmbeddedImage{ getDboPtr(trackEmbeddedImage) }
    {
    }

    Artwork::Artwork(ObjectPtr<Image> image)
        : _image{ getDboPtr(image) }
    {
    }

    Artwork::pointer Artwork::create(Session& session, ObjectPtr<TrackEmbeddedImage> trackEmbeddedImage)
    {
        session.checkWriteTransaction();
        return session.getDboSession()->add(std::unique_ptr<Artwork>{ new Artwork{ trackEmbeddedImage } });
    }

    Artwork::pointer Artwork::create(Session& session, ObjectPtr<Image> image)
    {
        session.checkWriteTransaction();
        return session.getDboSession()->add(std::unique_ptr<Artwork>{ new Artwork{ image } });
    }

    std::size_t Artwork::getCount(Session& session)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<long>("SELECT COUNT(*) FROM artwork"));
    }

    Artwork::pointer Artwork::find(Session& session, ArtworkId id)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<Artwork>>("SELECT a FROM artwork a").where("a.id = ?").bind(id));
    }

    Artwork::pointer Artwork::find(Session& session, TrackEmbeddedImageId id)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<Artwork>>("SELECT a FROM artwork a").where("a.track_embedded_image_id = ?").bind(id));
    }

    Artwork::pointer Artwork::find(Session& session, ImageId id)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<Artwork>>("SELECT a FROM artwork a").where("a.image_id = ?").bind(id));
    }

    Wt::WDateTime Artwork::getLastWrittenTime() const
    {
        auto query{ session()->query<Wt::WDateTime>("SELECT MAX(COALESCE(image.file_last_write, track.file_last_write)) AS last_written_datetime FROM artwork") };
        query.leftJoin("image ON artwork.image_id = image.id");
        query.leftJoin("track_embedded_image ON artwork.track_embedded_image_id = track_embedded_image.id");
        query.leftJoin("track_embedded_image_link ON track_embedded_image.id = track_embedded_image_link.track_embedded_image_id");
        query.leftJoin("track ON track.id = track_embedded_image_link.track_id");
        query.where("artwork.id = ?").bind(getId());

        return utils::fetchQuerySingleResult(query);
    }

    std::filesystem::path Artwork::getAbsoluteFilePath() const
    {
        auto query{ session()->query<std::filesystem::path>("SELECT COALESCE(image.absolute_file_path, track.absolute_file_path) AS absolute_file_path FROM artwork") };
        query.leftJoin("image ON artwork.image_id = image.id");
        query.leftJoin("track_embedded_image ON artwork.track_embedded_image_id = track_embedded_image.id");
        query.leftJoin("track_embedded_image_link ON track_embedded_image.id = track_embedded_image_link.track_embedded_image_id");
        query.leftJoin("track ON track.id = track_embedded_image_link.track_id");
        query.limit(1); // there may be several tracks matching this artwork...
        query.where("artwork.id = ?").bind(getId());

        return utils::fetchQuerySingleResult(query);
    }
} // namespace lms::db
