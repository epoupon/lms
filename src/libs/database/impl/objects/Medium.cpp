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

#include "ArtistDbo.hpp"
#include "ArtworkDbo.hpp"
#include "ClusterDbo.hpp"
#include "DirectoryDbo.hpp"
#include "GenreDbo.hpp"
#include "GroupingDbo.hpp"
#include "LanguageDbo.hpp"
#include "MediaLibraryDbo.hpp"
#include "MediumDbo.hpp"
#include "MoodDbo.hpp"
#include "MovementDbo.hpp"
#include "ObjectPtrImpl.hpp"
#include "ReleaseDbo.hpp"
#include "Session.hpp"
#include "TrackArtistLinkDbo.hpp"
#include "TrackDbo.hpp"
#include "TrackEmbeddedImageDbo.hpp"
#include "TrackEmbeddedImageLinkDbo.hpp"
#include "TrackLyricsDbo.hpp"
#include "Utils.hpp"
#include "WorkDbo.hpp"
#include "traits/IdTypeTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::Medium)

namespace lms::db
{
    dbo::Medium::Medium(const ObjectPtr<db::Release>& release)
        : _release(getDboPtr(release))
    {
    }

    Medium::pointer Medium::create(ISession& session, const ObjectPtr<Release>& release)
    {
        return utils::addObject(session, std::unique_ptr<dbo::Medium>{ new dbo::Medium{ release } });
    }

    void Medium::destroy(ISession& session, std::span<const MediumId> ids)
    {
        utils::destroyObjects<dbo::Medium>(session, ids);
    }

    std::size_t Medium::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM medium"));
    }

    Medium::pointer Medium::find(ISession& session, MediumId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleObject(getDboSession(session)->query<Wt::Dbo::ptr<dbo::Medium>>("SELECT m from medium m").where("m.id = ?").bind(id));
    }

    Medium::pointer Medium::find(ISession& session, ReleaseId releaseId, std::optional<std::size_t> position)
    {
        checkReadTransaction(session);

        std::optional<int> dbPosition;
        if (position)
            dbPosition = static_cast<int>(*position);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::Medium>>("SELECT m from medium m") };
        query.where("m.release_id = ?").bind(releaseId);
        if (position)
            query.where("m.position = ?").bind(*dbPosition);
        else
            query.where("m.position IS NULL");

        return utils::fetchQuerySingleObject(query);
    }

    void Medium::find(ISession& session, const IdRange<MediumId>& idRange, const std::function<void(const Medium::pointer&)>& func)
    {
        checkReadTransaction(session);

        assert(idRange.isValid());

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::Medium>>("SELECT m from medium m").orderBy("m.id").where("m.id BETWEEN ? AND ?").bind(idRange.first).bind(idRange.last) };

        utils::forEachQueryObject(query, [&](const Medium::pointer& medium) {
            func(medium);
        });
    }

    IdRange<MediumId> Medium::findNextIdRange(ISession& session, MediumId lastRetrievedId, std::size_t count)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<std::tuple<MediumId, MediumId>>("SELECT MIN(sub.id) AS first_id, MAX(sub.id) AS last_id FROM (SELECT m.id FROM medium m WHERE m.id > ? ORDER BY m.id LIMIT ?) sub") };
        query.bind(lastRetrievedId);
        query.bind(static_cast<int>(count));

        auto res{ utils::fetchQuerySingleResult(query) };
        return IdRange<MediumId>{ .first = std::get<0>(res), .last = std::get<1>(res) };
    }

    std::vector<MediumId> Medium::findOrphanIds(ISession& session, std::optional<Range> range)
    {
        checkReadTransaction(session);

        // select the mediums that have no track
        auto query{ getDboSession(session)->query<MediumId>("select m.id from medium m LEFT OUTER JOIN track t ON m.id = t.medium_id WHERE t.id IS NULL") };
        return utils::execRangeQuery<MediumId>(query, range);
    }

    void Medium::updatePreferredArtwork(ISession& session, MediumId mediumId, ArtworkId artworkId)
    {
        checkWriteTransaction(session);

        if (artworkId.isValid())
            utils::executeCommand(*getDboSession(session), "UPDATE medium SET preferred_artwork_id = ? WHERE id = ?", artworkId, mediumId);
        else
            utils::executeCommand(*getDboSession(session), "UPDATE medium SET preferred_artwork_id = NULL WHERE id = ?", mediumId);
    }

    ReleaseId Medium::getReleaseId() const
    {
        return getDbo(*this)._release.id();
    }

    ObjectPtr<Release> Medium::getRelease() const
    {
        return toObjectPtr(getDbo(*this)._release);
    }

    ObjectPtr<Artwork> Medium::getPreferredArtwork() const
    {
        return toObjectPtr(getDbo(*this)._preferredArtwork);
    }

    ArtworkId Medium::getPreferredArtworkId() const
    {
        return getDbo(*this)._preferredArtwork.id();
    }
} // namespace lms::db