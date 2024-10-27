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
#include <vector>

#include <gtest/gtest.h>

#include "Parser.hpp"

namespace lms::metadata
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

        bool hasEmbeddedCover() const override { return false; };

        const AudioProperties& getAudioProperties() const override { return audioProperties; }

    private:
        const Tags _tags;
        Performers _performers;
        ExtraUserTags _extraUserTags;
        LyricsTags _lyricsTags;
    };
} // namespace lms::metadata