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

#include "database/objects/PodcastEpisode.hpp"

#include "ObjectPtrImpl.hpp"

namespace lms::db::dbo
{
    class Artwork;
    class Podcast;

    class PodcastEpisode final : public db::PodcastEpisode, public Wt::Dbo::Dbo<PodcastEpisode>
    {
    public:
        PodcastEpisode() = default;
        using db::PodcastEpisode::PodcastEpisode;

        explicit PodcastEpisode(const ObjectPtr<db::Podcast>& podcast);

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _manualDownloadState, "manual_download_state");
            Wt::Dbo::field(a, _audioRelativeFilePath, "audio_relative_file_path");

            Wt::Dbo::field(a, _duration, "duration");
            Wt::Dbo::field(a, _container, "container");
            Wt::Dbo::field(a, _codec, "codec");
            Wt::Dbo::field(a, _bitrate, "bitrate");
            Wt::Dbo::field(a, _channelCount, "channel_count");
            Wt::Dbo::field(a, _sampleRate, "sample_rate");
            Wt::Dbo::field(a, _bitsPerSample, "bits_per_sample");

            Wt::Dbo::field(a, _title, "title");
            Wt::Dbo::field(a, _link, "link");
            Wt::Dbo::field(a, _description, "description");
            Wt::Dbo::field(a, _author, "author");
            Wt::Dbo::field(a, _category, "category");
            Wt::Dbo::field(a, _enclosureUrl, "enclosure_url");
            Wt::Dbo::field(a, _enclosureContentType, "enclosure_content_type");
            Wt::Dbo::field(a, _enclosureLength, "enclosure_size");
            Wt::Dbo::field(a, _pubDate, "pub_date");
            Wt::Dbo::field(a, _imageUrl, "image_url");
            Wt::Dbo::field(a, _subtitle, "subtitle");
            Wt::Dbo::field(a, _summary, "summary");
            Wt::Dbo::field(a, _explicit, "explicit");

            Wt::Dbo::belongsTo(a, _artwork, "artwork", Wt::Dbo::OnDeleteSetNull);
            Wt::Dbo::belongsTo(a, _podcast, "podcast", Wt::Dbo::OnDeleteCascade);
        }

        Wt::Dbo::ptr<Artwork> _artwork;
        Wt::Dbo::ptr<Podcast> _podcast;
    };
} // namespace lms::db::dbo

namespace lms::db
{
    template<>
    struct DboOf<PodcastEpisode>
    {
        using type = dbo::PodcastEpisode;
    };
} // namespace lms::db
