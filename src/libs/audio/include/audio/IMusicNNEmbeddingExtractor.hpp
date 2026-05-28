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

#include <filesystem>
#include <memory>

#include "audio/MusicNNEmbeddings.hpp"

namespace lms::audio
{
    class IMusicNNEmbeddingExtractor
    {
    public:
        virtual ~IMusicNNEmbeddingExtractor() = default;

        struct ExtractionResult
        {
            TrackMusicNNEmbeddings embeddings{};
            std::size_t patchCount{};
        };

        [[nodiscard]] virtual ExtractionResult extract(const std::filesystem::path& audioFile) const = 0;
    };

    bool canExtractMusicNNEmbeddings();
    std::unique_ptr<IMusicNNEmbeddingExtractor> createMusicNNEmbeddingExtractor(const std::filesystem::path& modelPath, std::size_t maxPatchCount);
    std::string getMusicNNModelIdentifier(const std::filesystem::path& modelPath);
} // namespace lms::audio
