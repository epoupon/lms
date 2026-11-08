/*
 * Copyright (C) 2020 Emeric Poupon
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

#include "database/objects/Podcast.hpp"

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "ArtworkDbo.hpp"
#include "ObjectPtrImpl.hpp"
#include "PodcastDbo.hpp"
#include "PodcastEpisodeDbo.hpp"
#include "Session.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/StringViewTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::Podcast)

namespace lms::db
{
    Podcast::Podcast(std::string_view url)
        : _url{ url }
    {
    }

    Podcast::pointer Podcast::create(ISession& session, std::string_view url)
    {
        return utils::addObject(session, std::unique_ptr<dbo::Podcast>{ new dbo::Podcast{ url } });
    }

    void Podcast::destroy(ISession& session, std::span<const PodcastId> ids)
    {
        utils::destroyObjects<dbo::Podcast>(session, ids);
    }

    std::size_t Podcast::getCount(ISession& session)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM podcast"));
    }

    Podcast::pointer Podcast::find(ISession& session, PodcastId id)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleObject(getDboSession(session)->query<Wt::Dbo::ptr<dbo::Podcast>>("SELECT p from podcast p").where("p.id = ?").bind(id));
    }

    Podcast::pointer Podcast::find(ISession& session, std::string_view url)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleObject(getDboSession(session)->query<Wt::Dbo::ptr<dbo::Podcast>>("SELECT p from podcast p").where("p.url = ?").bind(url));
    }

    void Podcast::find(ISession& session, std::function<void(const pointer&)> func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::Podcast>>("SELECT p from podcast p") };
        utils::forEachQueryObject(query, func);
    }

    ObjectPtr<Artwork> Podcast::getArtwork() const
    {
        return toObjectPtr(getDbo(*this)._artwork);
    }

    ArtworkId Podcast::getArtworkId() const
    {
        return getDbo(*this)._artwork.id();
    }

    void Podcast::setArtwork(const ObjectPtr<Artwork>& artwork)
    {
        getDbo(*this)._artwork = getDboPtr(artwork);
    }
} // namespace lms::db
