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

#include "database/Session.hpp"
#include "database/objects/Track.hpp"

#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::TrackMusicNNEmbeddings)

namespace lms::db
{
    TrackMusicNNEmbeddings::TrackMusicNNEmbeddings(ObjectPtr<Track> track)
        : _track{ getDboPtr(track) }
    {
    }

    TrackMusicNNEmbeddings::pointer TrackMusicNNEmbeddings::create(Session& session, ObjectPtr<Track> track)
    {
        return session.getDboSession()->add(std::unique_ptr<TrackMusicNNEmbeddings>{ new TrackMusicNNEmbeddings{ track } });
    }

    std::size_t TrackMusicNNEmbeddings::getCount(Session& session)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM track_musicnn_embeddings"));
    }

    TrackMusicNNEmbeddings::pointer TrackMusicNNEmbeddings::find(Session& session, TrackMusicNNEmbeddingsId id)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->find<TrackMusicNNEmbeddings>().where("id = ?").bind(id));
    }

    TrackMusicNNEmbeddings::pointer TrackMusicNNEmbeddings::find(Session& session, TrackId trackId)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->find<TrackMusicNNEmbeddings>().where("track_id = ?").bind(trackId));
    }

    RangeResults<TrackMusicNNEmbeddingsId> TrackMusicNNEmbeddings::find(Session& session, std::optional<Range> range)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<TrackMusicNNEmbeddingsId>("SELECT id from track_musicnn_embeddings") };

        return utils::execRangeQuery<TrackMusicNNEmbeddingsId>(query, range);
    }

    void TrackMusicNNEmbeddings::find(Session& session, std::function<void(const pointer&)> func)
    {
        auto query{ session.getDboSession()->find<TrackMusicNNEmbeddings>() };

        utils::forEachQueryResult(query, [&](const TrackMusicNNEmbeddings::pointer& embeddings) {
            func(embeddings);
        });
    }

    void TrackMusicNNEmbeddings::removeAll(Session& session)
    {
        session.checkWriteTransaction();
        utils::executeCommand(*session.getDboSession(), "DELETE FROM track_musicnn_embeddings");
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
