/*
 * Copyright (C) 2018 Emeric Poupon
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

#include "database/objects/TrackAudioFeatures.hpp"

#include <Wt/Dbo/Impl.h>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include "database/Session.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/Track.hpp"

#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::TrackAudioFeatures)

namespace lms::db
{
    TrackAudioFeatures::TrackAudioFeatures(ObjectPtr<Track> track)
        : _track{ getDboPtr(track) }
    {
    }

    TrackAudioFeatures::pointer TrackAudioFeatures::create(Session& session, ObjectPtr<Track> track)
    {
        return session.getDboSession()->add(std::unique_ptr<TrackAudioFeatures>{ new TrackAudioFeatures{ track } });
    }

    std::size_t TrackAudioFeatures::getCount(Session& session)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM track_audio_features"));
    }

    TrackAudioFeatures::pointer TrackAudioFeatures::find(Session& session, TrackAudioFeaturesId id)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->find<TrackAudioFeatures>().where("id = ?").bind(id));
    }

    TrackAudioFeatures::pointer TrackAudioFeatures::find(Session& session, TrackId trackId)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->find<TrackAudioFeatures>().where("track_id = ?").bind(trackId));
    }

    RangeResults<TrackAudioFeaturesId> TrackAudioFeatures::find(Session& session, std::optional<Range> range)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<TrackAudioFeaturesId>("SELECT id from track_audio_features") };

        return utils::execRangeQuery<TrackAudioFeaturesId>(query, range);
    }

    void TrackAudioFeatures::find(Session& session, std::function<void(const pointer&)> func)
    {
        auto query{ session.getDboSession()->find<TrackAudioFeatures>() };

        utils::forEachQueryResult(query, [&](const TrackAudioFeatures::pointer& features) {
            func(features);
        });
    }

    std::span<const std::byte> TrackAudioFeatures::getData() const
    {
        return std::span<const std::byte>{ reinterpret_cast<const std::byte*>(_data.data()), _data.size() };
    }

    void TrackAudioFeatures::setData(std::span<const std::byte> data)
    {
        const auto* start{ reinterpret_cast<const unsigned char*>(data.data()) };
        _data.assign(start, start + data.size());
    }
} // namespace lms::db
