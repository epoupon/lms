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

#include "database/objects/Medium.hpp"

#include <Wt/Dbo/Impl.h>

#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/TrackEmbeddedImage.hpp"
#include "database/objects/TrackEmbeddedImageLink.hpp"
#include "database/objects/TrackLyrics.hpp"

#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::Medium);

namespace lms::db
{
    Medium::Medium(ObjectPtr<Release> release)
        : _release(getDboPtr(release))
    {
    }

    Medium::pointer Medium::create(Session& session, ObjectPtr<Release> release)
    {
        return session.getDboSession()->add(std::unique_ptr<Medium>{ new Medium{ release } });
    }

    std::size_t Medium::getCount(Session& session)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM medium"));
    }

    Medium::pointer Medium::find(Session& session, MediumId id)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<Medium>>("SELECT m from medium m").where("m.id = ?").bind(id));
    }

    Medium::pointer Medium::find(Session& session, ReleaseId releaseId, std::optional<std::size_t> position)
    {
        session.checkReadTransaction();

        std::optional<int> dbPosition;
        if (position)
            dbPosition = static_cast<int>(*position);

        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<Medium>>("SELECT m from medium m") };
        query.where("m.release_id = ?").bind(releaseId);
        if (position)
            query.where("m.position = ?").bind(*dbPosition);
        else
            query.where("m.position IS NULL");

        return utils::fetchQuerySingleResult(query);
    }

} // namespace lms::db