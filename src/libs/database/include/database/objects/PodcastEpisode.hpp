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

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <Wt/Dbo/Field.h>
#include <Wt/Dbo/collection.h>
#include <Wt/WDateTime.h>

#include "core/media/Codec.hpp"
#include "core/media/Container.hpp"

#include "database/Object.hpp"
#include "database/Types.hpp"
#include "database/objects/ArtworkId.hpp"
#include "database/objects/PodcastEpisodeId.hpp"
#include "database/objects/PodcastId.hpp"
#include "database/objects/Types.hpp"
#include "database/objects/detail/Types.hpp"

namespace lms::db
{
    class Artwork;
    class ISession;
    class Podcast;

    class PodcastEpisode final : public Object<PodcastEpisode, PodcastEpisodeId>
    {
    public:
        enum class ManualDownloadState
        {
            None = 0,
            DownloadRequested = 1,
            DeleteRequested = 3,
        };

        struct FindParameters
        {
            PodcastEpisodeSortMode sortMode = PodcastEpisodeSortMode::None;
            db::PodcastId podcast; // if set, only episodes from this podcast
            std::optional<Range> range;
            std::optional<ManualDownloadState> manualDownloadState; // if set, only episodes that matches one of these states

            FindParameters& setSortMode(PodcastEpisodeSortMode _sortMode)
            {
                sortMode = _sortMode;
                return *this;
            }
            FindParameters& setPodcast(db::PodcastId _podcast)
            {
                podcast = _podcast;
                return *this;
            }
            FindParameters& setRange(const std::optional<Range>& _range)
            {
                range = _range;
                return *this;
            }
            FindParameters& setManualDownloadState(std::optional<ManualDownloadState> state)
            {
                manualDownloadState = state;
                return *this;
            }
        };

        PodcastEpisode() = default;

        static pointer create(ISession& session, ObjectPtr<Podcast> podcast);
        static std::size_t getCount(ISession& session);
        static pointer find(ISession& session, PodcastEpisodeId id);
        static pointer findNewtestEpisode(ISession& session, PodcastId id);
        static void find(ISession& session, const FindParameters& params, std::function<void(const pointer&)> func);

        // getters
        ManualDownloadState getManualDownloadState() const { return _manualDownloadState; }
        const std::filesystem::path& getAudioRelativeFilePath() const { return _audioRelativeFilePath; }

        // Audio properties
        std::chrono::milliseconds getDuration() const { return _duration; }
        std::optional<core::media::Container> getContainer() const;
        std::optional<core::media::Codec> getCodec() const;
        std::size_t getBitrate() const { return _bitrate; }
        std::size_t getChannelCount() const { return _channelCount; }
        std::size_t getSampleRate() const { return _sampleRate; }
        std::optional<std::size_t> getBitsPerSample() const { return _bitsPerSample; }

        std::string_view getTitle() const { return _title; }
        std::string_view getLink() const { return _link; }
        std::string_view getDescription() const { return _description; }
        std::string_view getAuthor() const { return _author; }
        std::string_view getCategory() const { return _category; }
        std::string_view getEnclosureUrl() const { return _enclosureUrl; }
        std::string_view getEnclosureContentType() const { return _enclosureContentType; }
        std::int64_t getEnclosureLength() const { return _enclosureLength; }
        const Wt::WDateTime& getPubDate() const { return _pubDate; }
        std::string_view getImageUrl() const { return _imageUrl; }
        std::string_view getSubtitle() const { return _subtitle; }
        std::string_view getSummary() const { return _summary; }
        bool isExplicit() const { return _explicit; }
        ObjectPtr<Podcast> getPodcast() const { return _podcast; }
        PodcastId getPodcastId() const { return _podcast.id(); }
        ObjectPtr<Artwork> getArtwork() const;
        ArtworkId getArtworkId() const;

        // setters
        void setManualDownloadState(ManualDownloadState state) { _manualDownloadState = state; }
        void setAudioRelativeFilePath(const std::filesystem::path& relativeFilePath) { _audioRelativeFilePath = relativeFilePath; }

        // Audio properties
        void setDuration(std::chrono::milliseconds duration) { _duration = duration; }
        void setContainer(core::media::Container container);
        void setCodec(core::media::Codec codec);
        void setBitrate(std::size_t bitrate) { _bitrate = bitrate; }
        void setChannelCount(std::size_t channelCount) { _channelCount = channelCount; }
        void setSampleRate(std::size_t sampleRate) { _sampleRate = sampleRate; }
        void setBitsPerSample(std::optional<std::size_t> bitsPerSample) { _bitsPerSample = bitsPerSample; }

        void setTitle(std::string_view title) { _title = title; }
        void setLink(std::string_view link) { _link = link; }
        void setDescription(std::string_view description) { _description = description; }
        void setAuthor(std::string_view author) { _author = author; }
        void setCategory(std::string_view category) { _category = category; }
        void setEnclosureUrl(std::string_view enclosureUrl) { _enclosureUrl = enclosureUrl; }
        void setEnclosureContentType(std::string_view enclosureContentType) { _enclosureContentType = enclosureContentType; }
        void setEnclosureLength(uint64_t enclosureLength) { _enclosureLength = enclosureLength; }
        void setPubDate(const Wt::WDateTime& pubDate) { _pubDate = pubDate; }
        void setImageUrl(std::string_view imageUrl) { _imageUrl = imageUrl; }
        void setSubtitle(std::string_view subtitle) { _subtitle = subtitle; }
        void setSummary(std::string_view summary) { _summary = summary; }
        void setExplicit(bool explicit_) { _explicit = explicit_; }
        void setDuration(std::chrono::duration<int, std::milli> duration) { _duration = duration; }
        void setArtwork(ObjectPtr<Artwork> artwork);

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

    private:
        PodcastEpisode(ObjectPtr<Podcast> podcast);

        ManualDownloadState _manualDownloadState{ ManualDownloadState::None };
        std::filesystem::path _audioRelativeFilePath; // relative to cache dir, only set if downloaded

        // Audio properties
        std::chrono::duration<int, std::milli> _duration{ 0 };
        detail::Container _container{ detail::Container::Unknown };
        detail::Codec _codec{ detail::Codec::Unknown };
        int _bitrate{}; // in bps
        int _channelCount{};
        int _sampleRate{};
        std::optional<int> _bitsPerSample;

        std::string _url;
        std::string _title;
        std::string _link;
        std::string _description;
        std::string _author;
        std::string _category;
        std::string _enclosureUrl;
        std::string _enclosureContentType;
        int _enclosureLength{ 0 };
        Wt::WDateTime _pubDate;

        // itunes fields
        std::string _imageUrl;
        std::string _subtitle;
        std::string _summary;
        bool _explicit{};

        Wt::Dbo::ptr<Artwork> _artwork;
        Wt::Dbo::ptr<Podcast> _podcast;
    };
} // namespace lms::db
