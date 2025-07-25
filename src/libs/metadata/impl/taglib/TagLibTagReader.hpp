/*
 * Copyright (C) 2018 Emeric Poupon
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

#include <filesystem>
#include <map>
#include <span>
#include <string>

#include <taglib/tfile.h>
#include <taglib/tpropertymap.h>

#include "ITagReader.hpp"

namespace lms::metadata::taglib
{
    class TagLibTagReader : public ITagReader
    {
    public:
        TagLibTagReader(const std::filesystem::path& path, ParserReadStyle parserReadStyle, bool debug);
        ~TagLibTagReader() override;
        TagLibTagReader(const TagLibTagReader&) = delete;
        TagLibTagReader& operator=(const TagLibTagReader&) = delete;

    private:
        void computeAudioProperties();
        void visitTagValues(TagType tag, TagValueVisitor visitor) const override;
        void visitTagValues(std::string_view tag, TagValueVisitor visitor) const override;
        void visitPerformerTags(PerformerVisitor visitor) const override;
        void visitLyricsTags(LyricsVisitor visitor) const override;

        const AudioProperties& getAudioProperties() const override { return _audioProperties; }

        std::unique_ptr<TagLib::File> _file;
        AudioProperties _audioProperties;
        TagLib::PropertyMap _propertyMap; // case-insensitive keys
        std::multimap<std::string /* language*/, std::string /* lyrics */> _id3v2Lyrics;
    };
} // namespace lms::metadata::taglib
