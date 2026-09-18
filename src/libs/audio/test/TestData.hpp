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

#include <chrono>
#include <filesystem>
#include <ostream>
#include <span>
#include <string_view>

#include "core/LiteralString.hpp"
#include "core/media/Codec.hpp"
#include "core/media/Container.hpp"

namespace lms::audio::tests
{
    // Every test audio file's embedded title/artist/album tags share these values
    inline constexpr core::LiteralString testTagTitle{ "Test Title" };
    inline constexpr core::LiteralString testTagArtist{ "Test Artist" };
    inline constexpr core::LiteralString testTagAlbum{ "Test Album" };

    struct TestAudioFile
    {
        std::string_view name; // must be a valid gtest identifier, used as the test/benchmark case name
        std::string_view fileName;
        core::media::Container container;
        core::media::Codec codec;
        unsigned channelCount;
        unsigned sampleRate; // the codec's actual internal rate (Opus is always 48000, regardless of the source)
        std::chrono::milliseconds duration;
        bool hasCoverArt;

        std::filesystem::path getPath() const;
    };

    std::ostream& operator<<(std::ostream& os, const TestAudioFile& file);

    std::span<const TestAudioFile> getTestAudioFiles();
} // namespace lms::audio::tests
