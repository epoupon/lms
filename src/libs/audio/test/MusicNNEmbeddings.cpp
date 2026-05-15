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

#include <numeric>
#include <vector>

#include <gtest/gtest.h>

#include "audio/Exception.hpp"
#include "audio/MusicNNEmbeddings.hpp"

namespace lms::audio::tests
{
    TEST(MusicNNEmbeddings, blobRoundTrip)
    {
        TrackMusicNNEmbeddings original{};

        std::iota(original.mean.values.begin(), original.mean.values.end(), 0.F);

        std::vector<std::byte> blob(sizeof(TrackMusicNNEmbeddings));
        trackMusicNNEmbeddingsToBlob(original, blob);

        TrackMusicNNEmbeddings restored{};
        trackMusicNNEmbeddingsFromBlob(blob, restored);

        for (std::size_t i{}; i < MusicNNEmbedding::size; ++i)
            EXPECT_FLOAT_EQ(restored.mean.values[i], original.mean.values[i]);
    }

    TEST(MusicNNEmbeddings, blobSizeTooSmallThrows)
    {
        TrackMusicNNEmbeddings embeddings{};
        std::vector<std::byte> blob(sizeof(TrackMusicNNEmbeddings) - 1);
        EXPECT_THROW(trackMusicNNEmbeddingsToBlob(embeddings, blob), Exception);
    }

    TEST(MusicNNEmbeddings, blobFromSizeTooSmallThrows)
    {
        std::vector<std::byte> blob(sizeof(TrackMusicNNEmbeddings) - 1, std::byte{});
        TrackMusicNNEmbeddings embeddings{};
        EXPECT_THROW(trackMusicNNEmbeddingsFromBlob(blob, embeddings), Exception);
    }
} // namespace lms::audio::tests
