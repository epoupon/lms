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

#include "database/objects/Release.hpp"

#include "ObjectPtrImpl.hpp"

namespace lms::db::dbo
{
    class Artwork;
    class Country;
    class Label;
    class Release;
    class ReleaseArtistLink;
    class ReleaseType;
    class Track;

    class Country final : public db::Country, public Wt::Dbo::Dbo<Country>
    {
    public:
        Country() = default;
        using db::Country::Country;

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _name, "name");
            Wt::Dbo::hasMany(a, _releases, Wt::Dbo::ManyToMany, "release_country", "", Wt::Dbo::OnDeleteCascade);
        }

        Wt::Dbo::collection<Wt::Dbo::ptr<Release>> _releases; // releases that match this country
    };

    class Label final : public db::Label, public Wt::Dbo::Dbo<Label>
    {
    public:
        Label() = default;
        using db::Label::Label;

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _name, "name");
            Wt::Dbo::hasMany(a, _releases, Wt::Dbo::ManyToMany, "release_label", "", Wt::Dbo::OnDeleteCascade);
        }

        Wt::Dbo::collection<Wt::Dbo::ptr<Release>> _releases; // releases that match this label
    };

    class ReleaseType final : public db::ReleaseType, public Wt::Dbo::Dbo<ReleaseType>
    {
    public:
        ReleaseType() = default;
        using db::ReleaseType::ReleaseType;

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _name, "name");
            Wt::Dbo::hasMany(a, _releases, Wt::Dbo::ManyToMany, "release_release_type", "", Wt::Dbo::OnDeleteCascade);
        }

        Wt::Dbo::collection<Wt::Dbo::ptr<Release>> _releases; // releases that match this type
    };

    class Release final : public db::Release, public Wt::Dbo::Dbo<Release>
    {
    public:
        Release() = default;
        using db::Release::Release;

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _name, "name");
            Wt::Dbo::field(a, _sortName, "sort_name");
            Wt::Dbo::field(a, _MBID, "mbid");
            Wt::Dbo::field(a, _groupMBID, "group_mbid");
            Wt::Dbo::field(a, _totalDisc, "total_disc");
            Wt::Dbo::field(a, _artistDisplayName, "artist_display_name");
            Wt::Dbo::field(a, _isCompilation, "is_compilation");
            Wt::Dbo::field(a, _barcode, "barcode");
            Wt::Dbo::field(a, _comment, "comment");

            Wt::Dbo::hasMany(a, _tracks, Wt::Dbo::ManyToOne, "release");
            Wt::Dbo::hasMany(a, _releaseArtistLinks, Wt::Dbo::ManyToOne, "release");
            Wt::Dbo::belongsTo(a, _preferredArtwork, "preferred_artwork", Wt::Dbo::OnDeleteSetNull);
            Wt::Dbo::hasMany(a, _labels, Wt::Dbo::ManyToMany, "release_label", "", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::hasMany(a, _releaseTypes, Wt::Dbo::ManyToMany, "release_release_type", "", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::hasMany(a, _countries, Wt::Dbo::ManyToMany, "release_country", "", Wt::Dbo::OnDeleteCascade);
        }

        Wt::Dbo::collection<Wt::Dbo::ptr<Track>> _tracks;
        Wt::Dbo::collection<Wt::Dbo::ptr<ReleaseArtistLink>> _releaseArtistLinks;
        Wt::Dbo::ptr<Artwork> _preferredArtwork;
        Wt::Dbo::collection<Wt::Dbo::ptr<Label>> _labels;
        Wt::Dbo::collection<Wt::Dbo::ptr<ReleaseType>> _releaseTypes;
        Wt::Dbo::collection<Wt::Dbo::ptr<Country>> _countries;
    };
} // namespace lms::db::dbo

namespace lms::db
{
    template<>
    struct DboOf<Country>
    {
        using type = dbo::Country;
    };

    template<>
    struct DboOf<Label>
    {
        using type = dbo::Label;
    };

    template<>
    struct DboOf<ReleaseType>
    {
        using type = dbo::ReleaseType;
    };

    template<>
    struct DboOf<Release>
    {
        using type = dbo::Release;
    };
} // namespace lms::db
