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

#include "audio/ITagReader.hpp"

#include "AudioFile.hpp"

namespace lms::audio::ffmpeg
{
    class TagReader : public ITagReader
    {
    public:
        TagReader(const AudioFile& audioFile, bool enableExtraDebugLogs);
        ~TagReader() override;
        TagReader(const TagReader&) = delete;
        TagReader& operator=(const TagReader&) = delete;

    private:
        void visitTagValues(TagType tag, TagValueVisitor visitor) const override;
        void visitTagValues(std::string_view tag, TagValueVisitor visitor) const override;
        void visitPerformerTags(PerformerVisitor visitor) const override;
        void visitLyricsTags(LyricsVisitor visitor) const override;

        const AudioFile& _audioFile;
        AudioFile::MetadataMap _metaDataMap;
    };
} // namespace lms::audio::ffmpeg
