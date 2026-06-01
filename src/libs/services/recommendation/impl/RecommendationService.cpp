/*
 * Copyright (C) 2019 Emeric Poupon
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

#include "RecommendationService.hpp"

#include <boost/asio/post.hpp>

#include "audio/IMusicNNEmbeddingExtractor.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"

#include "audio-similarity/musicnn/MusicNNEmbeddingEngine.hpp"
#include "clusters/ClustersEngine.hpp"

namespace lms::recommendation
{
    namespace
    {
        db::ScanSettings::RecommendationEngineType getRecommendationEngineType(db::Session& session)
        {
            auto transaction{ session.createReadTransaction() };
            return db::ScanSettings::find(session)->getRecommendationEngineType();
        }

        EngineType toEngineType(db::ScanSettings::RecommendationEngineType type)
        {
            switch (type)
            {
            case db::ScanSettings::RecommendationEngineType::None:
                return EngineType::None;
            case db::ScanSettings::RecommendationEngineType::Clusters:
                return EngineType::Clusters;
            case db::ScanSettings::RecommendationEngineType::AudioSimilarity:
                return EngineType::AudioSimilarity;
            }
            return EngineType::None;
        }

        std::unique_ptr<IEngine> createEngine(db::ScanSettings::RecommendationEngineType type, db::IDb& db)
        {
            switch (type)
            {
            case db::ScanSettings::RecommendationEngineType::Clusters:
                return std::make_unique<ClusterEngine>(db);
            case db::ScanSettings::RecommendationEngineType::AudioSimilarity:
                return std::make_unique<MusicNNEmbeddingEngine>(db);
            case db::ScanSettings::RecommendationEngineType::None:
                return nullptr;
            }
            return nullptr;
        }
    } // namespace

    std::unique_ptr<IRecommendationService> createRecommendationService(db::IDb& db)
    {
        return std::make_unique<RecommendationService>(db);
    }

    RecommendationService::RecommendationService(db::IDb& db)
        : _db{ db }
        , _ioContextRunner{ _ioContext, 1, "RecommendationEngine" }
    {
        requestReload();
    }

    TrackResults RecommendationService::findSimilarTracks(db::TrackListId trackListId, std::size_t maxCount) const
    {
        std::shared_lock lock{ _mutex, std::try_to_lock };
        if (!lock || !_engine)
            return {};

        return _engine->findSimilarTracksFromTrackList(trackListId, maxCount);
    }

    TrackResults RecommendationService::findSimilarTracks(std::span<const db::TrackId> trackIds, std::size_t maxCount) const
    {
        std::shared_lock lock{ _mutex, std::try_to_lock };
        if (!lock || !_engine)
            return {};

        return _engine->findSimilarTracks(trackIds, maxCount);
    }

    ReleaseResults RecommendationService::findSimilarReleases(db::ReleaseId releaseId, std::size_t maxCount) const
    {
        std::shared_lock lock{ _mutex, std::try_to_lock };
        if (!lock || !_engine)
            return {};

        return _engine->findSimilarReleases(releaseId, maxCount);
    }

    ArtistResults RecommendationService::findSimilarArtists(db::ArtistId artistId, core::EnumSet<db::TrackArtistLinkType> linkTypes, std::size_t maxCount) const
    {
        std::shared_lock lock{ _mutex, std::try_to_lock };
        if (!lock || !_engine)
            return {};

        return _engine->findSimilarArtists(artistId, linkTypes, maxCount);
    }

    TrackResults RecommendationService::findTrackSimilarityPath(db::TrackId startTrackId, db::TrackId endTrackId, std::size_t maxCount) const
    {
        std::shared_lock lock{ _mutex, std::try_to_lock };
        if (!lock || !_engine)
            return {};

        return _engine->findTrackSimilarityPath(startTrackId, endTrackId, maxCount);
    }

    bool RecommendationService::isEngineTypeSupported(EngineType type) const
    {
        switch (type)
        {
        case EngineType::AudioSimilarity:
            return audio::canExtractMusicNNEmbeddings();

        case EngineType::None:
        case EngineType::Clusters:
            return true;
        }

        return false;
    }

    db::ScanSettings::RecommendationEngineType RecommendationService::prepareReload()
    {
        const auto type{ getRecommendationEngineType(_db.getTLSSession()) };
        std::unique_lock lock{ _mutex };
        _engineType = toEngineType(type);
        _engine.reset();
        return type;
    }

    EngineType RecommendationService::getEngineType() const
    {
        std::shared_lock lock{ _mutex };
        return _engineType;
    }

    void RecommendationService::requestReload()
    {
        const auto type{ prepareReload() };

        boost::asio::post(_ioContext, [this, type] {
            auto newEngine{ createEngine(type, _db) };
            if (!newEngine)
                return;
            newEngine->load();

            std::unique_lock lock{ _mutex };
            _engine = std::move(newEngine);
        });
    }

    bool RecommendationService::isLoaded() const
    {
        std::shared_lock lock{ _mutex, std::try_to_lock };
        return lock && _engine != nullptr;
    }
} // namespace lms::recommendation
