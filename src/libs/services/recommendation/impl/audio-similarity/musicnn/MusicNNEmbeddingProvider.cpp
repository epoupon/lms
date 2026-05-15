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

#include "MusicNNEmbeddingProvider.hpp"

#include "audio/MusicNNEmbeddings.hpp"
#include "database/Session.hpp"
#include "database/objects/TrackMusicNNEmbeddings.hpp"

namespace lms::recommendation
{
    namespace
    {
        void readEmbeddings(const db::ObjectPtr<db::TrackMusicNNEmbeddings>& dbEmbeddings, MusicNNEmbeddingProvider::Vector& vec)
        {
            static_assert(MusicNNEmbeddingProvider::Vector::getSize() == MusicNNEmbeddingProvider::DimCount);

            audio::TrackMusicNNEmbeddings embeddings{};
            audio::trackMusicNNEmbeddingsFromBlob(dbEmbeddings->getData(), embeddings);

            std::size_t outputIndex{};

            for (float val : embeddings.mean.values)
                vec[outputIndex++] = val;

            assert(outputIndex == MusicNNEmbeddingProvider::DimCount);
        }
    } // namespace

    bool MusicNNEmbeddingProvider::getVector(db::Session& session, db::TrackId trackId, Vector& vec)
    {
        session.checkReadTransaction();

        const db::TrackMusicNNEmbeddings::pointer embeddings{ db::TrackMusicNNEmbeddings::find(session, trackId) };
        if (embeddings)
            readEmbeddings(embeddings, vec);

        return embeddings;
    }

    void MusicNNEmbeddingProvider::visitVectors(db::Session& session, const std::function<void(db::TrackId, Vector&)>& visitor)
    {
        session.checkReadTransaction();

        Vector vec;
        db::TrackMusicNNEmbeddings::find(session, [&](const db::TrackMusicNNEmbeddings::pointer& embeddings) {
            readEmbeddings(embeddings, vec);
            visitor(embeddings->getTrackId(), vec);
        });
    }
} // namespace lms::recommendation
