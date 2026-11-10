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

#include "database/objects/TrackMusicNNEmbeddings.hpp"

#include <Wt/Dbo/Impl.h>

#include "DboAccess.hpp"
#include "Session.hpp"
#include "TrackDbo.hpp"
#include "TrackMusicNNEmbeddingsDbo.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::TrackMusicNNEmbeddings)

namespace lms::db
{
    dbo::TrackMusicNNEmbeddings::TrackMusicNNEmbeddings(Wt::Dbo::ptr<Track> track)
        : _track{ std::move(track) }
    {
    }

    TrackMusicNNEmbeddings TrackMusicNNEmbeddings::create(ISession& session, TrackId track)
    {
        return utils::addObject(session, std::unique_ptr<dbo::TrackMusicNNEmbeddings>{ new dbo::TrackMusicNNEmbeddings{ getDboSession(session)->loadLazy<dbo::Track>(track.getValue()) } });
    }

    void TrackMusicNNEmbeddings::destroy(ISession& session, std::span<const TrackMusicNNEmbeddingsId> ids)
    {
        utils::destroyObjects<dbo::TrackMusicNNEmbeddings>(session, ids);
    }

    std::size_t TrackMusicNNEmbeddings::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM track_musicnn_embeddings"));
    }

    std::optional<TrackMusicNNEmbeddings> TrackMusicNNEmbeddings::find(ISession& session, TrackMusicNNEmbeddingsId id)
    {
        checkReadTransaction(session);

        if (!id.isValid())
            return std::nullopt;

        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::TrackMusicNNEmbeddings>().where("id = ?").bind(id));
    }

    std::optional<TrackMusicNNEmbeddings> TrackMusicNNEmbeddings::find(ISession& session, TrackId trackId)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::TrackMusicNNEmbeddings>().where("track_id = ?").bind(trackId));
    }

    std::vector<TrackMusicNNEmbeddingsId> TrackMusicNNEmbeddings::find(ISession& session, std::optional<Range> range)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<TrackMusicNNEmbeddingsId>("SELECT id from track_musicnn_embeddings") };

        return utils::execRangeQuery<TrackMusicNNEmbeddingsId>(query, range);
    }

    void TrackMusicNNEmbeddings::find(ISession& session, const std::function<void(const TrackMusicNNEmbeddings&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->find<dbo::TrackMusicNNEmbeddings>() };

        utils::forEachQueryObject(query, func);
    }

    void TrackMusicNNEmbeddings::removeAll(ISession& session)
    {
        checkWriteTransaction(session);
        utils::executeCommand(*getDboSession(session), "DELETE FROM track_musicnn_embeddings");
    }

    std::span<const std::byte> TrackMusicNNEmbeddings::getData() const
    {
        return std::span<const std::byte>{ reinterpret_cast<const std::byte*>(_data.data()), _data.size() };
    }

    void TrackMusicNNEmbeddings::setData(std::span<const std::byte> data)
    {
        const auto* start{ reinterpret_cast<const unsigned char*>(data.data()) };
        _data.assign(start, start + data.size());
    }

    void TrackMusicNNEmbeddings::update(ISession& session, const TrackMusicNNEmbeddings& trackMusicNNEmbeddings)
    {
        utils::updateObject<dbo::TrackMusicNNEmbeddings>(session, trackMusicNNEmbeddings);
    }
} // namespace lms::db
