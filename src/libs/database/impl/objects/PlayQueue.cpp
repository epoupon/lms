/*
 * Copyright (C) 2024 Emeric Poupon
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

#include "database/objects/PlayQueue.hpp"

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

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
#include "database/objects/User.hpp"

#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/StringViewTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::PlayQueue)

namespace lms::db
{
    PlayQueue::PlayQueue(const ObjectPtr<User>& user, std::string_view name)
        : _name{ name }
        , _user{ getDboPtr(user) }
    {
    }

    PlayQueue::pointer PlayQueue::create(Session& session, const ObjectPtr<User>& user, std::string_view name)
    {
        return session.getDboSession()->add(std::unique_ptr<PlayQueue>{ new PlayQueue{ user, name } });
    }

    std::size_t PlayQueue::getCount(Session& session)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM playqueue"));
    }

    PlayQueue::pointer PlayQueue::find(Session& session, PlayQueueId id)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<PlayQueue>>("SELECT p from playqueue p").where("p.id = ?").bind(id));
    }

    PlayQueue::pointer PlayQueue::find(Session& session, UserId userId, std::string_view name)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<PlayQueue>>("SELECT p from playqueue p") };
        query.where("p.user_id = ?").bind(userId);
        query.where("p.name = ?").bind(name);

        return utils::fetchQuerySingleResult(query);
    }

    void PlayQueue::clear()
    {
        _tracks.clear();
    }

    void PlayQueue::addTrack(const ObjectPtr<Track>& track)
    {
        _tracks.insert(getDboPtr(track));
    }

    Track::pointer PlayQueue::getTrackAtCurrentIndex() const
    {
        auto query{ _tracks.find() };
        query.offset(_currentIndex);
        query.limit(1);

        return utils::fetchQuerySingleResult(query);
    }

    void PlayQueue::visitTracks(const std::function<void(const ObjectPtr<Track>& track)>& visitor) const
    {
        utils::forEachQueryResult(_tracks.find(), visitor);
    }

} // namespace lms::db
