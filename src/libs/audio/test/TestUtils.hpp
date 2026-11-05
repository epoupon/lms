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

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

#include "core/ScopedTmpDirectory.hpp"

namespace lms::audio::tests
{
    using core::ScopedTmpDirectory;

    void writeFile(const std::filesystem::path& path, std::span<const std::byte> data);

    // Decodes the whole file to interleaved 16-bit PCM
    std::vector<std::int16_t> decodeInterleaved(const std::filesystem::path& path, unsigned channelCount, unsigned sampleRate);
} // namespace lms::audio::tests
