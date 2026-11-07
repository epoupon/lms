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

#include "database/objects/Artwork.hpp"
#include "database/objects/PodcastEpisode.hpp"

#include "Session.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/StringViewTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::Podcast)

namespace lms::db
{
    Podcast::Podcast(std::string_view url)
        : _url{ url }
    {
    }

    Podcast::pointer Podcast::create(ISession& session, std::string_view url)
    {
        return utils::addObject(session, std::unique_ptr<Podcast>{ new Podcast{ url } });
    }

    void Podcast::destroy(ISession& session, std::span<const PodcastId> ids)
    {
        utils::destroyObjects<Podcast>(session, ids);
    }

    std::size_t Podcast::getCount(ISession& session)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM podcast"));
    }

    Podcast::pointer Podcast::find(ISession& session, PodcastId id)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<Wt::Dbo::ptr<Podcast>>("SELECT p from podcast p").where("p.id = ?").bind(id));
    }

    Podcast::pointer Podcast::find(ISession& session, std::string_view url)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<Wt::Dbo::ptr<Podcast>>("SELECT p from podcast p").where("p.url = ?").bind(url));
    }

    void Podcast::find(ISession& session, std::function<void(const pointer&)> func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<Podcast>>("SELECT p from podcast p") };
        utils::forEachQueryResult(query, func);
    }

    ObjectPtr<Artwork> Podcast::getArtwork() const
    {
        return _artwork;
    }

    ArtworkId Podcast::getArtworkId() const
    {
        return _artwork.id();
    }

    void Podcast::setArtwork(const ObjectPtr<Artwork>& artwork)
    {
        _artwork = getDboPtr(artwork);
    }
} // namespace lms::db
