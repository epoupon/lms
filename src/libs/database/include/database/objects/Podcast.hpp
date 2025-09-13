/*
 * Copyright (C) 2025 Emeric Poupon
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

#include <functional>
#include <string>
#include <string_view>

#include <Wt/Dbo/Field.h>
#include <Wt/Dbo/collection.h>
#include <Wt/WDateTime.h>

#include "database/Object.hpp"
#include "database/objects/ArtworkId.hpp"
#include "database/objects/PodcastId.hpp"

namespace lms::db
{
    class Artwork;
    class PodcastEpisode;
    class Session;

    class Podcast final : public Object<Podcast, PodcastId>
    {
    public:
        static const std::size_t maxMediaLength{ 64 };

        Podcast() = default;
        static std::size_t getCount(Session& session);
        static pointer find(Session& session, PodcastId id);
        static pointer find(Session& session, std::string_view url);
        static void find(Session& session, std::function<void(const pointer&)> func);

        // getters
        std::string_view getUrl() const { return _url; }

        bool isDeleteRequested() const { return _deleteRequested; }
        std::string_view getTitle() const { return _title; }
        std::string_view getLink() const { return _link; }
        std::string_view getDescription() const { return _description; }
        std::string_view getLanguage() const { return _language; }
        std::string_view getCopyright() const { return _copyright; }
        Wt::WDateTime getLastBuildDate() const { return _lastBuildDate; }
        std::string_view getAuthor() const { return _author; }
        std::string_view getCategory() const { return _category; }
        bool isExplicit() const { return _explicit; }
        std::string_view getImageUrl() const { return _imageUrl; }
        std::string_view getOwnerEmail() const { return _ownerEmail; }
        std::string_view getOwnerName() const { return _ownerName; }
        std::string_view getSubtitle() const { return _subtitle; }
        std::string_view getSummary() const { return _summary; }
        ObjectPtr<Artwork> getArtwork() const;
        ArtworkId getArtworkId() const;

        // setters
        void setUrl(std::string_view url) { _url = url; }

        void setDeleteRequested(bool deleteRequested) { _deleteRequested = deleteRequested; }
        void setTitle(std::string_view title) { _title = title; }
        void setLink(std::string_view link) { _link = link; }
        void setDescription(std::string_view description) { _description = description; }
        void setLanguage(std::string_view language) { _language = language; }
        void setCopyright(std::string_view copyright) { _copyright = copyright; }
        void setLastBuildDate(const Wt::WDateTime& lastBuildDate) { _lastBuildDate = lastBuildDate; }
        void setAuthor(std::string_view author) { _author = author; }
        void setCategory(std::string_view category) { _category = category; }
        void setExplicit(bool explicit_) { _explicit = explicit_; }
        void setImageUrl(std::string_view imageUrl) { _imageUrl = imageUrl; }
        void setOwnerEmail(std::string_view ownerEmail) { _ownerEmail = ownerEmail; }
        void setOwnerName(std::string_view ownerName) { _ownerName = ownerName; }
        void setSubtitle(std::string_view subtitle) { _subtitle = subtitle; }
        void setSummary(std::string_view summary) { _summary = summary; }
        void setArtwork(ObjectPtr<Artwork> artwork);

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

    private:
        friend class Session;
        Podcast(std::string_view url);
        static pointer create(Session& session, std::string_view url);

        std::string _url;

        bool _deleteRequested{};
        std::string _title;
        std::string _link;
        std::string _description;
        std::string _language;
        std::string _copyright;
        Wt::WDateTime _lastBuildDate;

        // itunes fields
        std::string _author;
        std::string _category;
        bool _explicit{};
        std::string _imageUrl;
        std::string _ownerEmail;
        std::string _ownerName;
        std::string _subtitle;
        std::string _summary;

        Wt::Dbo::ptr<Artwork> _artwork;
        Wt::Dbo::collection<Wt::Dbo::ptr<PodcastEpisode>> _episodes;
    };
} // namespace lms::db
