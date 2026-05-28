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

#include "audio/IMusicNNEmbeddingExtractor.hpp"

#include <array>
#include <fstream>
#include <span>
#include <string>

#include "core/XxHash3.hpp"

#if LMS_HAVE_ONNX_RUNTIME
    #include "musicnn/MusicNNEmbeddingExtractor.hpp"
#endif

namespace lms::audio
{
    bool canExtractMusicNNEmbeddings()
    {
#if LMS_HAVE_ONNX_RUNTIME
        return true;
#else
        return false;
#endif
    }

    std::unique_ptr<IMusicNNEmbeddingExtractor> createMusicNNEmbeddingExtractor([[maybe_unused]] const std::filesystem::path& modelPath, std::size_t maxPatchCount)
    {
#if LMS_HAVE_ONNX_RUNTIME
        return std::make_unique<musicnn::MusicNNEmbeddingExtractor>(modelPath, maxPatchCount);
#else
        return {};
#endif
    }

    std::string getMusicNNModelIdentifier(const std::filesystem::path& modelPath)
    {
        std::ifstream file{ modelPath, std::ios::binary };
        if (!file)
            return {};

        core::XxHash3_64 hasher;
        constexpr std::size_t readBufSize{ 65536 };
        std::array<char, readBufSize> buf{};
        while (file.read(buf.data(), buf.size()) || file.gcount() > 0)
            hasher.update(std::as_bytes(std::span{ buf.data(), static_cast<std::size_t>(file.gcount()) }));

        if (!file.eof())
            return {};

        return std::to_string(hasher.digest());
    }
} // namespace lms::audio
