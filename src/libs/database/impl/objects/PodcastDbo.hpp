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

#pragma once

#include <Wt/Dbo/Dbo.h>
#include <Wt/Dbo/Field.h>
#include <Wt/Dbo/collection.h>

#include "database/objects/Podcast.hpp"

#include "ObjectPtrImpl.hpp"

namespace lms::db::dbo
{
    class Artwork;
    class PodcastEpisode;

    class Podcast final : public db::Podcast, public Wt::Dbo::Dbo<Podcast>
    {
    public:
        Podcast() = default;
        using db::Podcast::Podcast;

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _url, "url");

            Wt::Dbo::field(a, _deleteRequested, "delete_requested");
            Wt::Dbo::field(a, _title, "title");
            Wt::Dbo::field(a, _link, "link");
            Wt::Dbo::field(a, _description, "description");
            Wt::Dbo::field(a, _language, "language");
            Wt::Dbo::field(a, _copyright, "copyright");
            Wt::Dbo::field(a, _lastBuildDate, "last_build_date");

            Wt::Dbo::field(a, _author, "author");
            Wt::Dbo::field(a, _category, "category");
            Wt::Dbo::field(a, _explicit, "explicit");
            Wt::Dbo::field(a, _imageUrl, "image_url");
            Wt::Dbo::field(a, _ownerEmail, "owner_email");
            Wt::Dbo::field(a, _ownerName, "owner_name");
            Wt::Dbo::field(a, _subtitle, "subtitle");
            Wt::Dbo::field(a, _summary, "summary");

            Wt::Dbo::belongsTo(a, _artwork, "artwork", Wt::Dbo::OnDeleteSetNull);
            Wt::Dbo::hasMany(a, _episodes, Wt::Dbo::ManyToOne, "podcast");
        }

        Wt::Dbo::ptr<Artwork> _artwork;
        Wt::Dbo::collection<Wt::Dbo::ptr<PodcastEpisode>> _episodes;
    };
} // namespace lms::db::dbo

namespace lms::db
{
    template<>
    struct DboOf<Podcast>
    {
        using type = dbo::Podcast;
    };
} // namespace lms::db
