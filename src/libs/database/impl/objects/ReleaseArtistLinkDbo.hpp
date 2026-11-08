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

#include "database/objects/ReleaseArtistLink.hpp"

#include "ObjectPtrImpl.hpp"

namespace lms::db::dbo
{
    class Artist;
    class Release;

    class ReleaseArtistLink final : public db::ReleaseArtistLink, public Wt::Dbo::Dbo<ReleaseArtistLink>
    {
    public:
        ReleaseArtistLink() = default;
        using db::ReleaseArtistLink::ReleaseArtistLink;

        ReleaseArtistLink(const ObjectPtr<db::Release>& release, const ObjectPtr<db::Artist>& artist, bool artistMBIDMatched);

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _artistName, "artist_name");
            Wt::Dbo::field(a, _artistSortName, "artist_sort_name");
            Wt::Dbo::field(a, _artistMBIDMatched, "artist_mbid_matched");

            Wt::Dbo::belongsTo(a, _release, "release", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::belongsTo(a, _artist, "artist", Wt::Dbo::OnDeleteCascade);
        }

        Wt::Dbo::ptr<Release> _release;
        Wt::Dbo::ptr<Artist> _artist;
    };
} // namespace lms::db::dbo

namespace lms::db
{
    template<>
    struct DboOf<ReleaseArtistLink>
    {
        using type = dbo::ReleaseArtistLink;
    };
} // namespace lms::db
