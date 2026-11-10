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

#include "ArtistDbo.hpp"
#include "ArtworkDbo.hpp"
#include "ClusterDbo.hpp"
#include "DboAccess.hpp"
#include "DirectoryDbo.hpp"
#include "GenreDbo.hpp"
#include "GroupingDbo.hpp"
#include "LanguageDbo.hpp"
#include "MediaLibraryDbo.hpp"
#include "MediumDbo.hpp"
#include "MoodDbo.hpp"
#include "MovementDbo.hpp"
#include "PlayQueueDbo.hpp"
#include "ReleaseDbo.hpp"
#include "Session.hpp"
#include "TrackArtistLinkDbo.hpp"
#include "TrackDbo.hpp"
#include "TrackEmbeddedImageDbo.hpp"
#include "TrackEmbeddedImageLinkDbo.hpp"
#include "TrackLyricsDbo.hpp"
#include "UserDbo.hpp"
#include "Utils.hpp"
#include "WorkDbo.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/StringViewTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::PlayQueue)

namespace lms::db
{
    PlayQueue::PlayQueue(std::string_view name)
        : _name{ name }
    {
    }

    dbo::PlayQueue::PlayQueue(Wt::Dbo::ptr<User> user, std::string_view name)
        : db::PlayQueue{ name }
        , _user{ std::move(user) }
    {
    }

    PlayQueue PlayQueue::create(ISession& session, UserId user, std::string_view name)
    {
        return utils::addObject(session, std::unique_ptr<dbo::PlayQueue>{ new dbo::PlayQueue{ getDboSession(session)->loadLazy<dbo::User>(user.getValue()), name } });
    }

    void PlayQueue::destroy(ISession& session, std::span<const PlayQueueId> ids)
    {
        utils::destroyObjects<dbo::PlayQueue>(session, ids);
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

    std::optional<PlayQueue> PlayQueue::find(ISession& session, UserId userId, std::string_view name)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::PlayQueue>>("SELECT p from playqueue p") };
        query.where("p.user_id = ?").bind(userId);
        query.where("p.name = ?").bind(name);

        return utils::fetchQuerySingleObject(query);
    }

    bool PlayQueue::isEmpty(ISession& session, PlayQueueId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT EXISTS (SELECT 1 FROM playqueue_track WHERE playqueue_id = ?)").bind(id)) == 0;
    }

    void PlayQueue::clear(ISession& session, PlayQueueId id)
    {
        utils::modifyObject<dbo::PlayQueue>(session, id)._tracks.clear();
    }

    void PlayQueue::addTrack(ISession& session, PlayQueueId id, TrackId track)
    {
        dbo::PlayQueue& playQueue{ utils::modifyObject<dbo::PlayQueue>(session, id) };
        playQueue._tracks.insert(getDboSession(session)->loadLazy<dbo::Track>(track.getValue()));
    }

    void PlayQueue::update(ISession& session, const PlayQueue& playQueue)
    {
        utils::updateObject<dbo::PlayQueue>(session, playQueue);
    }
} // namespace lms::db
