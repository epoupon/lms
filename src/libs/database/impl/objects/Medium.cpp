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

    void Medium::find(Session& session, const IdRange<MediumId>& idRange, const std::function<void(const Medium::pointer&)>& func)
    {
        assert(idRange.isValid());

        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<Medium>>("SELECT m from medium m").orderBy("m.id").where("m.id BETWEEN ? AND ?").bind(idRange.first).bind(idRange.last) };

        utils::forEachQueryResult(query, [&](const Medium::pointer& medium) {
            func(medium);
        });
    }

    IdRange<MediumId> Medium::findNextIdRange(Session& session, MediumId lastRetrievedId, std::size_t count)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<std::tuple<MediumId, MediumId>>("SELECT MIN(sub.id) AS first_id, MAX(sub.id) AS last_id FROM (SELECT m.id FROM medium m WHERE m.id > ? ORDER BY m.id LIMIT ?) sub") };
        query.bind(lastRetrievedId);
        query.bind(static_cast<int>(count));

        auto res{ utils::fetchQuerySingleResult(query) };
        return IdRange<MediumId>{ .first = std::get<0>(res), .last = std::get<1>(res) };
    }

    RangeResults<MediumId> Medium::findOrphanIds(Session& session, std::optional<Range> range)
    {
        session.checkReadTransaction();

        // select the mediums that have no track
        auto query{ session.getDboSession()->query<MediumId>("select m.id from medium m LEFT OUTER JOIN track t ON m.id = t.medium_id WHERE t.id IS NULL") };
        return utils::execRangeQuery<MediumId>(query, range);
    }

    void Medium::updatePreferredArtwork(Session& session, MediumId mediumId, ArtworkId artworkId)
    {
        session.checkWriteTransaction();

        if (artworkId.isValid())
            utils::executeCommand(*session.getDboSession(), "UPDATE medium SET preferred_artwork_id = ? WHERE id = ?", artworkId, mediumId);
        else
            utils::executeCommand(*session.getDboSession(), "UPDATE medium SET preferred_artwork_id = NULL WHERE id = ?", mediumId);
    }

} // namespace lms::db