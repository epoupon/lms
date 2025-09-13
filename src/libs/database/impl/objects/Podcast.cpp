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

#include "database/Session.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/PodcastEpisode.hpp"

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

    Podcast::pointer Podcast::create(Session& session, std::string_view url)
    {
        return session.getDboSession()->add(std::unique_ptr<Podcast>{ new Podcast{ url } });
    }

    std::size_t Podcast::getCount(Session& session)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM podcast"));
    }

    Podcast::pointer Podcast::find(Session& session, PodcastId id)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<Podcast>>("SELECT p from podcast p").where("p.id = ?").bind(id));
    }

    Podcast::pointer Podcast::find(Session& session, std::string_view url)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<Podcast>>("SELECT p from podcast p").where("p.url = ?").bind(url));
    }

    void Podcast::find(Session& session, std::function<void(const pointer&)> func)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<Podcast>>("SELECT p from podcast p") };
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

    void Podcast::setArtwork(ObjectPtr<Artwork> artwork)
    {
        _artwork = getDboPtr(artwork);
    }
} // namespace lms::db
