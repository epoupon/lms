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

#include <map>
#include <string>

#include <taglib/tpropertymap.h>

#include "audio/ITagReader.hpp"

namespace TagLib
{
    class File;
}

namespace lms::audio::taglib
{
    class TagReader : public ITagReader
    {
    public:
        TagReader(::TagLib::File& file, bool enableExtraDebugLogs);
        ~TagReader() override;

        TagReader(const TagReader&) = delete;
        TagReader& operator=(const TagReader&) = delete;

    private:
        void visitTagValues(TagType tag, TagValueVisitor visitor) const override;
        void visitTagValues(std::string_view tag, TagValueVisitor visitor) const override;
        void visitPerformerTags(PerformerVisitor visitor) const override;
        void visitLyricsTags(LyricsVisitor visitor) const override;

        ::TagLib::File& _file;
        ::TagLib::PropertyMap _propertyMap; // case-insensitive keys
        std::multimap<std::string /* language*/, std::string /* lyrics */> _id3v2Lyrics;
    };
} // namespace lms::audio::taglib
