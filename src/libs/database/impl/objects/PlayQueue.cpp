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
#include "DirectoryDbo.hpp"
#include "GenreDbo.hpp"
#include "GroupingDbo.hpp"
#include "LanguageDbo.hpp"
#include "MediaLibraryDbo.hpp"
#include "MediumDbo.hpp"
#include "MoodDbo.hpp"
#include "MovementDbo.hpp"
#include "ObjectPtrImpl.hpp"
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

    dbo::PlayQueue::PlayQueue(const ObjectPtr<db::User>& user, std::string_view name)
        : db::PlayQueue{ name }
        , _user{ getDboPtr(user) }
    {
    }

    PlayQueue::pointer PlayQueue::create(ISession& session, const ObjectPtr<User>& user, std::string_view name)
    {
        return utils::addObject(session, std::unique_ptr<dbo::PlayQueue>{ new dbo::PlayQueue{ user, name } });
    }

    std::size_t PlayQueue::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM playqueue"));
    }

    PlayQueue::pointer PlayQueue::find(ISession& session, PlayQueueId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleObject(getDboSession(session)->query<Wt::Dbo::ptr<dbo::PlayQueue>>("SELECT p from playqueue p").where("p.id = ?").bind(id));
    }

    PlayQueue::pointer PlayQueue::find(ISession& session, UserId userId, std::string_view name)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::PlayQueue>>("SELECT p from playqueue p") };
        query.where("p.user_id = ?").bind(userId);
        query.where("p.name = ?").bind(name);

        return utils::fetchQuerySingleObject(query);
    }

    void PlayQueue::clear()
    {
        getDbo(*this)._tracks.clear();
    }

    void PlayQueue::addTrack(const ObjectPtr<Track>& track)
    {
        getDbo(*this)._tracks.insert(getDboPtr(track));
    }

    Track::pointer PlayQueue::getTrackAtCurrentIndex() const
    {
        auto query{ getDbo(*this)._tracks.find() };
        query.offset(_currentIndex);
        query.limit(1);

        return utils::fetchQuerySingleObject(query);
    }

    void PlayQueue::visitTracks(const std::function<void(const ObjectPtr<Track>& track)>& visitor) const
    {
        utils::forEachQueryObject(getDbo(*this)._tracks.find(), visitor);
    }

    ObjectPtr<User> PlayQueue::getUser() const
    {
        return toObjectPtr(getDbo(*this)._user);
    }

    bool PlayQueue::isEmpty() const
    {
        return getDbo(*this)._tracks.empty();
    }
} // namespace lms::db
