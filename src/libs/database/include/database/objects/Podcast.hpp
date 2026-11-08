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
#include <span>
#include <string>
#include <string_view>

#include <Wt/WDateTime.h>

#include "database/Object.hpp"
#include "database/objects/ArtworkId.hpp"
#include "database/objects/PodcastId.hpp"

namespace lms::db
{
    class Artwork;
    class ISession;

    class Podcast : public Object<Podcast, PodcastId>
    {
    public:
        static const std::size_t maxMediaLength{ 64 };

        static pointer create(ISession& session, std::string_view url);
        static void destroy(ISession& session, std::span<const PodcastId> ids);
        static std::size_t getCount(ISession& session);
        static pointer find(ISession& session, PodcastId id);
        static pointer find(ISession& session, std::string_view url);
        static void find(ISession& session, std::function<void(const pointer&)> func);

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
        void setArtwork(const ObjectPtr<Artwork>& artwork);

    protected:
        Podcast() = default;
        explicit Podcast(std::string_view url);

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
    };
} // namespace lms::db
