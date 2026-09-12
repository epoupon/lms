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

#include <functional>
#include <span>
#include <string_view>

#include "core/Utils.hpp"
#include "core/media/Codec.hpp"
#include "core/media/Container.hpp"

namespace lms::core::media
{
    using ExtensionSpan = std::span<const std::string_view>; // leading dot (ex: ".flac")

    struct AudioFormat
    {
        Container container;
        Codec codec;
    };

    // visitor returns core::Break to stop the iteration early
    void visitAudioFormats(const std::function<core::VisitorResult(const AudioFormat&, ExtensionSpan)>& visitor);

    // extension must be in canonical form (leading dot, lowercase, ex: ".flac")
    void visitAudioFormatsForExtension(std::string_view extension, const std::function<void(const AudioFormat&)>& visitor);

    // Returns the extensions registered for exactly this audio format (empty if none)
    ExtensionSpan getExtensionsForAudioFormat(AudioFormat format);
} // namespace lms::core::media
