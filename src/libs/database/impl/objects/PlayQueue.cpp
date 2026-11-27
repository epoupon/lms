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

#include "database/objects/TrackList.hpp"

#include "DboAccess.hpp"
#include "PlayQueueDbo.hpp"
#include "Session.hpp"
#include "TrackListDbo.hpp"
#include "UserDbo.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::PlayQueue)

namespace lms::db
{
    PlayQueue::PlayQueue(PlayQueueType type)
        : _type{ type }
    {
    }

    dbo::PlayQueue::PlayQueue(Wt::Dbo::ptr<User> user, PlayQueueType type, Wt::Dbo::ptr<TrackList> trackList)
        : db::PlayQueue{ type }
        , _user{ std::move(user) }
        , _trackList{ std::move(trackList) }
    {
    }

    PlayQueue PlayQueue::create(ISession& session, UserId user, PlayQueueType type)
    {
        TrackList trackList{ TrackList::create(session, "", TrackListType::Internal) };
        trackList.setUser(user);
        TrackList::update(session, trackList);

        Wt::Dbo::Session& dboSession{ *getDboSession(session) };
        return utils::addObject(session, std::unique_ptr<dbo::PlayQueue>{ new dbo::PlayQueue{ dboSession.loadLazy<dbo::User>(user.getValue()), type, dboSession.loadLazy<dbo::TrackList>(trackList.getId().getValue()) } });
    }

    void PlayQueue::destroy(ISession& session, std::span<const PlayQueueId> ids)
    {
        std::vector<TrackListId> trackListIds;
        trackListIds.reserve(ids.size());
        for (const PlayQueueId id : ids)
        {
            if (const std::optional<PlayQueue> playQueue{ find(session, id) })
                trackListIds.push_back(playQueue->getTrackListId());
        }

        utils::destroyObjects<dbo::PlayQueue>(session, ids);
        TrackList::destroy(session, trackListIds);
    }

    std::size_t PlayQueue::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM playqueue"));
    }

    std::optional<PlayQueue> PlayQueue::find(ISession& session, PlayQueueId id)
    {
        checkReadTransaction(session);

        if (!id.isValid())
            return std::nullopt;

        return utils::fetchQuerySingleObject(getDboSession(session)->query<Wt::Dbo::ptr<dbo::PlayQueue>>("SELECT p from playqueue p").where("p.id = ?").bind(id));
    }

    std::optional<PlayQueue> PlayQueue::find(ISession& session, UserId userId, PlayQueueType type)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::PlayQueue>>("SELECT p from playqueue p") };
        query.where("p.user_id = ?").bind(userId);
        query.where("p.type = ?").bind(type);

        return utils::fetchQuerySingleObject(query);
    }

    std::vector<PlayQueueId> PlayQueue::findIds(ISession& session, UserId userId)
    {
        checkReadTransaction(session);

        return utils::fetchQueryResults(getDboSession(session)->query<PlayQueueId>("SELECT id FROM playqueue").where("user_id = ?").bind(userId).orderBy("id"));
    }

    void PlayQueue::update(ISession& session, const PlayQueue& playQueue)
    {
        utils::updateObject<dbo::PlayQueue>(session, playQueue);
    }
} // namespace lms::db
