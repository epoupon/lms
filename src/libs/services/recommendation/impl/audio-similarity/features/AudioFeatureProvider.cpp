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

#include "AudioFeatureProvider.hpp"

#include "audio/AudioFeatures.hpp"
#include "database/Session.hpp"
#include "database/objects/TrackAudioFeatures.hpp"

namespace lms::recommendation
{
    namespace
    {
        void flattenTrackAudioFeatures(const audio::TrackAudioFeatures& trackFeatures, AudioFeatureProvider::Vector& vec)
        {
            static_assert(AudioFeatureProvider::mfccCoeffCount <= audio::AudioFeatures::mfccCount);
            std::size_t featureIndex{};

            for (std::size_t i{}; i < audio::AudioFeatures::melBandCount; ++i)
                vec[featureIndex++] = trackFeatures.mean.logMelStdDev[i];
            for (std::size_t i{}; i < audio::AudioFeatures::melBandCount; ++i)
                vec[featureIndex++] = trackFeatures.mean.logMelMean[i];

            for (std::size_t i{}; i < AudioFeatureProvider::mfccCoeffCount; ++i)
                vec[featureIndex++] = trackFeatures.mean.mfccMean[i];
            for (std::size_t i{}; i < AudioFeatureProvider::mfccCoeffCount; ++i)
                vec[featureIndex++] = trackFeatures.mean.mfccStdDev[i];

            vec[featureIndex++] = trackFeatures.mean.spectralCentroidMean;
            vec[featureIndex++] = trackFeatures.mean.spectralCentroidStdDev;

            vec[featureIndex++] = trackFeatures.mean.spectralRolloffMean;
            vec[featureIndex++] = trackFeatures.mean.spectralRolloffStdDev;

            vec[featureIndex++] = trackFeatures.mean.onsetStrengthMean;
            vec[featureIndex++] = trackFeatures.mean.onsetStrengthStdDev;

            vec[featureIndex++] = trackFeatures.mean.zeroCrossingRateMean;
            vec[featureIndex++] = trackFeatures.mean.zeroCrossingRateStdDev;

            assert(featureIndex == AudioFeatureProvider::featureCount);
        }

        void readFeatures(const db::ObjectPtr<db::TrackAudioFeatures>& dbFeatures, AudioFeatureProvider::Vector& vec)
        {
            audio::TrackAudioFeatures trackFeatures{};
            audio::trackAudioFeaturesFromBlob(dbFeatures->getData(), trackFeatures);
            flattenTrackAudioFeatures(trackFeatures, vec);
        }
    } // namespace

    bool AudioFeatureProvider::getVector(db::Session& session, db::TrackId trackId, Vector& vec)
    {
        session.checkReadTransaction();

        const db::TrackAudioFeatures::pointer features{ db::TrackAudioFeatures::find(session, trackId) };
        if (features)
            readFeatures(features, vec);

        return features;
    }

    void AudioFeatureProvider::visitVectors(db::Session& session, const std::function<void(db::TrackId, Vector&)>& visitor)
    {
        session.checkReadTransaction();

        Vector vec;
        db::TrackAudioFeatures::find(session, [&](const db::TrackAudioFeatures::pointer& features) {
            readFeatures(features, vec);
            visitor(features->getTrackId(), vec);
        });
    }
} // namespace lms::recommendation