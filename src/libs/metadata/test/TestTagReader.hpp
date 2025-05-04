/*
 * Copyright (C) 2024 Emeric Poupon
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

#include <map>
#include <memory>
#include <vector>

#include "ITagReader.hpp"

namespace lms::metadata::tests
{
    class TestTagReader : public ITagReader
    {
    public:
        static constexpr AudioProperties audioProperties{
            .bitrate = 128000,
            .bitsPerSample = 16,
            .channelCount = 2,
            .duration = std::chrono::seconds{ 180 },
            .sampleRate = 44000,
        };

        using Tags = std::unordered_map<TagType, std::vector<std::string_view>>;
        using Performers = std::unordered_map<std::string_view /*role*/, std::vector<std::string_view> /*names*/>;
        using ExtraUserTags = std::unordered_map<std::string_view, std::vector<std::string_view>>;
        using LyricsTags = std::unordered_map<std::string_view /*language*/, std::string_view /*contents*/>;
        TestTagReader(Tags&& tags)
            : _tags{ std::move(tags) }
        {
        }
        ~TestTagReader() override = default;
        TestTagReader(const TestTagReader&) = delete;
        TestTagReader& operator=(const TestTagReader&) = delete;

        void setPerformersTags(Performers&& performers)
        {
            _performers = std::move(performers);
        }

        void setExtraUserTags(ExtraUserTags&& extraUserTags)
        {
            _extraUserTags = std::move(extraUserTags);
        }

        void setLyricsTags(LyricsTags&& lyricsTags)
        {
            _lyricsTags = std::move(lyricsTags);
        }

        void visitTagValues(TagType tag, TagValueVisitor visitor) const override
        {
            auto itValues{ _tags.find(tag) };
            if (itValues != std::cend(_tags))
            {
                for (std::string_view value : itValues->second)
                    visitor(value);
            }
        }
        void visitTagValues(std::string_view tag, TagValueVisitor visitor) const override
        {
            auto itValues{ _extraUserTags.find(tag) };
            if (itValues == std::cend(_extraUserTags))
                return;

            for (std::string_view value : itValues->second)
                visitor(value);
        }

        void visitPerformerTags(PerformerVisitor visitor) const override
        {
            for (const auto& [role, names] : _performers)
            {
                for (const auto& name : names)
                    visitor(role, name);
            }
        }

        void visitLyricsTags(LyricsVisitor visitor) const override
        {
            for (const auto& [language, lyrics] : _lyricsTags)
                visitor(language, lyrics);
        }

        const AudioProperties& getAudioProperties() const override { return audioProperties; }

    private:
        const Tags _tags;
        Performers _performers;
        ExtraUserTags _extraUserTags;
        LyricsTags _lyricsTags;
    };

    inline std::unique_ptr<ITagReader> createDefaultPopulatedTestTagReader()
    {
        std::unique_ptr<TestTagReader> testTags{ std::make_unique<TestTagReader>(
            TestTagReader::Tags{
                { TagType::AcoustID, { "e987a441-e134-4960-8019-274eddacc418" } },
                { TagType::Advisory, { "2" } },
                { TagType::Album, { "MyAlbum" } },
                { TagType::AlbumSortOrder, { "MyAlbumSortName" } },
                { TagType::Artist, { "MyArtist1 & MyArtist2" } },
                { TagType::Artists, { "MyArtist1", "MyArtist2" } },
                { TagType::ArtistSortOrder, { "MyArtist1SortName", "MyArtist2SortName" } },
                { TagType::AlbumArtist, { "MyAlbumArtist1 & MyAlbumArtist2" } },
                { TagType::AlbumArtists, { "MyAlbumArtist1", "MyAlbumArtist2" } },
                { TagType::AlbumArtistsSortOrder, { "MyAlbumArtist1SortName", "MyAlbumArtist2SortName" } },
                { TagType::AlbumComment, { "MyAlbumComment" } },
                { TagType::Barcode, { "MyBarcode" } },
                { TagType::Comment, { "Comment1", "Comment2" } },
                { TagType::Compilation, { "1" } },
                { TagType::Composer, { "MyComposer1", "MyComposer2" } },
                { TagType::ComposerSortOrder, { "MyComposerSortOrder1", "MyComposerSortOrder2" } },
                { TagType::Conductor, { "MyConductor1", "MyConductor2" } },
                { TagType::Copyright, { "MyCopyright" } },
                { TagType::CopyrightURL, { "MyCopyrightURL" } },
                { TagType::Date, { "2020/03/04" } },
                { TagType::DiscNumber, { "2" } },
                { TagType::DiscSubtitle, { "MySubtitle" } },
                { TagType::Genre, { "Genre1", "Genre2" } },
                { TagType::Grouping, { "Grouping1", "Grouping2" } },
                { TagType::Media, { "CD" } },
                { TagType::Mixer, { "MyMixer1", "MyMixer2" } },
                { TagType::Mood, { "Mood1", "Mood2" } },
                { TagType::MusicBrainzArtistID, { "9d2e0c8c-8c5e-4372-a061-590955eaeaae", "5e2cf87f-c8d7-4504-8a86-954dc0840229" } },
                { TagType::MusicBrainzTrackID, { "0afb190a-6735-46df-a16d-199f48206e4a" } },
                { TagType::MusicBrainzReleaseArtistID, { "6fbf097c-1487-43e8-874b-50dd074398a7", "5ed3d6b3-2aed-4a03-828c-3c4d4f7406e1" } },
                { TagType::MusicBrainzReleaseID, { "3fa39992-b786-4585-a70e-85d5cc15ef69" } },
                { TagType::MusicBrainzReleaseGroupID, { "5b1a5a44-8420-4426-9b86-d25dc8d04838" } },
                { TagType::MusicBrainzRecordingID, { "bd3fc666-89de-4ac8-93f6-2dbf028ad8d5" } },
                { TagType::Producer, { "MyProducer1", "MyProducer2" } },
                { TagType::Remixer, { "MyRemixer1", "MyRemixer2" } },
                { TagType::RecordLabel, { "Label1", "Label2" } },
                { TagType::ReleaseCountry, { "MyCountry1", "MyCountry2" } },
                { TagType::Language, { "Language1", "Language2" } },
                { TagType::Lyricist, { "MyLyricist1", "MyLyricist2" } },
                { TagType::OriginalReleaseDate, { "2019/02/03" } },
                { TagType::ReleaseType, { "Album", "Compilation" } },
                { TagType::ReplayGainTrackGain, { "-0.33" } },
                { TagType::ReplayGainAlbumGain, { "-0.5" } },
                { TagType::TrackTitle, { "MyTitle" } },
                { TagType::TrackNumber, { "7" } },
                { TagType::TotalTracks, { "12" } },
                { TagType::TotalDiscs, { "3" } },
            }) };
        testTags->setExtraUserTags({ { "MY_AWESOME_TAG_A", { "MyTagValue1ForTagA", "MyTagValue2ForTagA" } },
            { "MY_AWESOME_TAG_B", { "MyTagValue1ForTagB", "MyTagValue2ForTagB" } } });
        testTags->setPerformersTags({ { "RoleA", { "MyPerformer1ForRoleA", "MyPerformer2ForRoleA" } },
            { "RoleB", { "MyPerformer1ForRoleB", "MyPerformer2ForRoleB" } } });
        testTags->setLyricsTags({ { "eng", "[00:00.00]First line\n[00:01.00]Second line" } });

        return testTags;
    }
} // namespace lms::metadata::tests