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

#include <boost/asio/io_context.hpp>

#include "core/IOContextRunner.hpp"

#include "database/Object.hpp"
#include "database/objects/TrackId.hpp"

#include "FeaturesDefs.hpp"
#include "IEngine.hpp"

namespace lms::db
{
    class Session;
    class TrackAudioFeatures;
} // namespace lms::db

namespace lms::recommendation
{
    class FeaturesEngine : public IEngine
    {
    public:
        FeaturesEngine(db::IDb& db);
        ~FeaturesEngine() override;

        FeaturesEngine(const FeaturesEngine&) = delete;
        FeaturesEngine& operator=(const FeaturesEngine&) = delete;

    private:
        void requestReload() override;

        TrackContainer findSimilarTracksFromTrackList(db::TrackListId tracklistId, std::size_t maxCount) const override;
        TrackContainer findSimilarTracks(const std::vector<db::TrackId>& tracksId, std::size_t maxCount) const override;
        ReleaseContainer getSimilarReleases(db::ReleaseId releaseId, std::size_t maxCount) const override;
        ArtistContainer getSimilarArtists(db::ArtistId artistId, core::EnumSet<db::TrackArtistLinkType> linkTypes, std::size_t maxCount) const override;

        void abort();
        void train();

        void computeDatasetStats();
        static void getAudioFeatureVector(const db::ObjectPtr<db::TrackAudioFeatures>& features, AudioFeatureVector& inputVector);
        void getNormalizedAudioFeatureVector(const db::ObjectPtr<db::TrackAudioFeatures>& features, AudioFeatureVector& inputVector) const;
        void trainSom();

        db::IDb& _db;
        bool _abortRequested{};
        boost::asio::io_context _ioContext;
        core::IOContextRunner _ioContextRunner;

        // Stats, used to normalize input data
        std::size_t _trackCount{};
        AudioFeatureVector _featureMeans;
        AudioFeatureVector _featureStdDevs;
        AudioSom _som;
        som::Matrix<std::vector<db::TrackId>> _neuronTrackMap;
    };
} // namespace lms::recommendation
