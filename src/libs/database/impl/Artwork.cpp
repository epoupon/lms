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

#include "database/Artwork.hpp"

#include "database/Image.hpp"
#include "database/Session.hpp"
#include "database/TrackEmbeddedImage.hpp"

#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

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
        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<Artwork>>("SELECT a FROM artwork a JOIN track_embedded_image t_e_i ON a.track_embedded_image_id = t_e_i.id").where("t_e_i.id = ?").bind(id));
    }

    Artwork::pointer Artwork::find(Session& session, ImageId id)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<Artwork>>("SELECT a FROM artwork a JOIN image i ON a.image_id = i.id").where("i.id = ?").bind(id));
    }

} // namespace lms::db
