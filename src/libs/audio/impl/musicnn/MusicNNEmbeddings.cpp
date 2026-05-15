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

#include "audio/MusicNNEmbeddings.hpp"

#include <bit>
#include <cstring>
#include <limits>

#include "audio/Exception.hpp"

namespace lms::audio
{
    static_assert(sizeof(float) == 4);
    static_assert(std::numeric_limits<float>::is_iec559);

    namespace
    {
        constexpr uint32_t byteswap32(uint32_t x)
        {
            return (x >> 24) | ((x >> 8) & 0x0000FF00u) | ((x << 8) & 0x00FF0000u) | (x << 24);
        }

        void writeFloats(std::span<const float> data, std::span<std::byte> blob)
        {
            if (blob.size() < data.size() * sizeof(uint32_t))
                throw Exception{ "Buffer too small to write MusicNN embeddings" };

            for (std::size_t i{}; i < data.size(); ++i)
            {
                uint32_t bits{ std::bit_cast<uint32_t>(data[i]) };
                if constexpr (std::endian::native == std::endian::little)
                    bits = byteswap32(bits);
                std::memcpy(blob.data() + i * 4, &bits, 4);
            }
        }

        void readFloats(std::span<const std::byte> blob, std::span<float> data)
        {
            if (blob.size() < data.size() * sizeof(uint32_t))
                throw Exception{ "Buffer too small to read MusicNN embeddings" };

            for (std::size_t i{}; i < data.size(); ++i)
            {
                uint32_t bits{};
                std::memcpy(&bits, blob.data() + i * 4, 4);
                if constexpr (std::endian::native == std::endian::little)
                    bits = byteswap32(bits);
                data[i] = std::bit_cast<float>(bits);
            }
        }
    } // anonymous namespace

    void trackMusicNNEmbeddingsToBlob(const TrackMusicNNEmbeddings& embeddings, std::span<std::byte> buffer)
    {
        if (buffer.size() < sizeof(TrackMusicNNEmbeddings))
            throw Exception{ "Buffer too small to write TrackMusicNNEmbeddings" };

        writeFloats(embeddings.mean.values, buffer.subspan(0, MusicNNEmbedding::size * sizeof(float)));
    }

    void trackMusicNNEmbeddingsFromBlob(std::span<const std::byte> buffer, TrackMusicNNEmbeddings& embeddings)
    {
        if (buffer.size() < sizeof(TrackMusicNNEmbeddings))
            throw Exception{ "Buffer too small to read TrackMusicNNEmbeddings" };

        readFloats(buffer.subspan(0, MusicNNEmbedding::size * sizeof(float)), embeddings.mean.values);
    }
} // namespace lms::audio
