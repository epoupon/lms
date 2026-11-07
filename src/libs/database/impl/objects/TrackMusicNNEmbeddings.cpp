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

#include "database/objects/Genre.hpp"
#include "database/objects/Grouping.hpp"
#include "database/objects/Language.hpp"
#include "database/objects/Mood.hpp"
#include "database/objects/Track.hpp"

#include "ObjectPtrImpl.hpp"
#include "Session.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::TrackMusicNNEmbeddings)

namespace lms::db
{
    TrackMusicNNEmbeddings::TrackMusicNNEmbeddings(const ObjectPtr<Track>& track)
        : _track{ getDboPtr(track) }
    {
    }

    TrackMusicNNEmbeddings::pointer TrackMusicNNEmbeddings::create(ISession& session, const ObjectPtr<Track>& track)
    {
        return utils::addObject(session, std::unique_ptr<TrackMusicNNEmbeddings>{ new TrackMusicNNEmbeddings{ track } });
    }

    std::size_t TrackMusicNNEmbeddings::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM track_musicnn_embeddings"));
    }

    TrackMusicNNEmbeddings::pointer TrackMusicNNEmbeddings::find(ISession& session, TrackMusicNNEmbeddingsId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->find<TrackMusicNNEmbeddings>().where("id = ?").bind(id));
    }

    TrackMusicNNEmbeddings::pointer TrackMusicNNEmbeddings::find(ISession& session, TrackId trackId)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->find<TrackMusicNNEmbeddings>().where("track_id = ?").bind(trackId));
    }

    std::vector<TrackMusicNNEmbeddingsId> TrackMusicNNEmbeddings::find(ISession& session, std::optional<Range> range)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<TrackMusicNNEmbeddingsId>("SELECT id from track_musicnn_embeddings") };

        return utils::execRangeQuery<TrackMusicNNEmbeddingsId>(query, range);
    }

    void TrackMusicNNEmbeddings::find(ISession& session, std::function<void(const pointer&)> func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->find<TrackMusicNNEmbeddings>() };

        utils::forEachQueryResult(query, [&](const TrackMusicNNEmbeddings::pointer& embeddings) {
            func(embeddings);
        });
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
} // namespace lms::db
