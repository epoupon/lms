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

#include "ScanStepExtractAudioFeatures.hpp"

#include <deque>

#include "audio/AudioFeatures.hpp"
#include "audio/Exception.hpp"
#include "core/IJob.hpp"
#include "core/IJobScheduler.hpp"
#include "core/ILogger.hpp"

#include "audio/IAudioFeaturesExtractor.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Track.hpp"

#include "JobQueue.hpp"
#include "ScanContext.hpp"
#include "database/objects/TrackAudioFeatures.hpp"
#include <optional>

namespace lms::scanner
{
    namespace
    {
        struct TrackInfo
        {
            db::TrackId track;
            std::filesystem::path trackPath;
        };

        struct TrackAudioFeatureAssociation
        {
            db::TrackId trackId;
            std::optional<audio::AudioFeatures> audioFeatures;
        };
        using TrackAudioFeatureAssociationContainer = std::deque<TrackAudioFeatureAssociation>;

        db::Track::FindParameters createFindTrackFeaturesParameters()
        {
            db::Track::FindParameters params;
            params.setHasAudioFeatures(false);
            params.setSortMethod(db::TrackSortMethod::Id);
            params.setRange(db::Range{ .offset = 0, .size = 1 });

            return params;
        }

        bool fetchNextTrackWithoutFeatures(db::Session& session, db::TrackId& lastRetrievedTrackId, TrackInfo& trackInfo)
        {
            auto transaction{ session.createReadTransaction() };

            db::Track::FindParameters params{ createFindTrackFeaturesParameters() };
            params.setLastTrackId(lastRetrievedTrackId);

            trackInfo.track = db::TrackId{};
            trackInfo.trackPath.clear();
            db::Track::find(session, params, [&](const db::Track::pointer& track) {
                trackInfo.track = track->getId();
                trackInfo.trackPath = track->getAbsoluteFilePath();
            });

            lastRetrievedTrackId = trackInfo.track;
            return trackInfo.track.isValid();
        }

        class ExtractAudioFeaturesJob : public core::IJob
        {
        public:
            ExtractAudioFeaturesJob(const audio::IAudioFeaturesExtractor& featuresExtractor, const TrackInfo& trackInfo)
                : _featuresExtractor{ featuresExtractor }
                , _trackInfo{ trackInfo }
            {
            }
            ~ExtractAudioFeaturesJob() override = default;
            ExtractAudioFeaturesJob(const ExtractAudioFeaturesJob&) = delete;
            ExtractAudioFeaturesJob& operator=(const ExtractAudioFeaturesJob&) = delete;

            const TrackInfo& getTrackInfo() const { return _trackInfo; }
            const audio::AudioFeatures* getAudioFeatures() const { return _extractedFeatures ? &_extractedFeatures.value() : nullptr; }
            std::string_view getErrorMessage() const { return _errorMessage; }

        private:
            core::LiteralString getName() const override { return "Extract Audio Features"; }

            void run() override
            {
                // TODO check for abort!!
                try
                {
                    LMS_LOG(DBUPDATER, DEBUG, "Extracting audio features for " << _trackInfo.trackPath);
                    _extractedFeatures.emplace(_featuresExtractor.extractFeatures(_trackInfo.trackPath).features);
                    LMS_LOG(DBUPDATER, DEBUG, "Extracting audio features complete for " << _trackInfo.trackPath);
                }
                catch (const audio::Exception& e)
                {
                    _errorMessage = e.what();
                }
            }

            const audio::IAudioFeaturesExtractor& _featuresExtractor;
            const TrackInfo _trackInfo;
            std::optional<audio::AudioFeatures> _extractedFeatures;
            std::string _errorMessage;
        };

        void updateTrackAudioFeatures(db::Session& session, const TrackAudioFeatureAssociation& trackAudioFeatureAssociation)
        {
            if (db::Track::pointer track{ db::Track::find(session, trackAudioFeatureAssociation.trackId) })
            {
                std::vector<std::byte> blob(sizeof(audio::AudioFeatures));
                audio::audioFeaturesToBlob(*trackAudioFeatureAssociation.audioFeatures, blob);
                db::TrackAudioFeatures::pointer trackFeatures{ session.create<db::TrackAudioFeatures>(track) };
                trackFeatures.modify()->setData(blob);
            }
        }

        void updateTrackAudioFeatures(db::Session& session, TrackAudioFeatureAssociationContainer& trackAudioFeatureAssociations, bool forceFullBatch)
        {
            constexpr std::size_t writeBatchSize{ 20 };

            while ((forceFullBatch && trackAudioFeatureAssociations.size() >= writeBatchSize) || (!forceFullBatch && trackAudioFeatureAssociations.empty()))
            {
                auto transaction{ session.createWriteTransaction() };

                for (std::size_t i{}; !trackAudioFeatureAssociations.empty() && i < writeBatchSize; ++i)
                {
                    updateTrackAudioFeatures(session, trackAudioFeatureAssociations.front());
                    trackAudioFeatureAssociations.pop_front();
                }
            }
        }
    } // namespace

    ScanStepExtractAudioFeatures::ScanStepExtractAudioFeatures(InitParams& initParams)
        : ScanStepBase{ initParams }
        , _featuresExtractor{ audio::createAudioFeaturesExtractor() }
    {
    }

    ScanStepExtractAudioFeatures::~ScanStepExtractAudioFeatures() = default;

    bool ScanStepExtractAudioFeatures::needProcess([[maybe_unused]] const ScanContext& context) const
    {
        return true;
    }

    void ScanStepExtractAudioFeatures::process(ScanContext& context)
    {
        db::Session& dbSession{ _db.getTLSSession() };

        {
            db::Track::FindParameters params{ createFindTrackFeaturesParameters() };

            auto transaction{ dbSession.createReadTransaction() };
            context.currentStepStats.totalElems = db::Track::getCount(dbSession, params);
        }

        TrackAudioFeatureAssociationContainer trackAudioFeatureAssociations;

        auto processResults = [&](std::span<std::unique_ptr<core::IJob>> jobs) {
            if (_abortScan)
                return;

            for (const auto& job : jobs)
            {
                const auto& extractAudioFeaturesJob{ static_cast<const ExtractAudioFeaturesJob&>(*job) };

                if (const audio::AudioFeatures * features{ extractAudioFeaturesJob.getAudioFeatures() })
                    trackAudioFeatureAssociations.push_back(TrackAudioFeatureAssociation{ .trackId = extractAudioFeaturesJob.getTrackInfo().track, .audioFeatures = *features });
                else
                    addError<AudioFeaturesExtractError>(context, extractAudioFeaturesJob.getTrackInfo().trackPath, extractAudioFeaturesJob.getErrorMessage());
            }

            updateTrackAudioFeatures(dbSession, trackAudioFeatureAssociations, true);

            context.currentStepStats.processedElems += jobs.size();
            _progressCallback(context.currentStepStats);
        };

        {
            JobQueue queue{ getJobScheduler(), 20, processResults, 10, 0.85F };

            db::TrackId lastRetrievedTrackId;
            TrackInfo trackInfo;
            while (!_abortScan && fetchNextTrackWithoutFeatures(dbSession, lastRetrievedTrackId, trackInfo))
                queue.push(std::make_unique<ExtractAudioFeaturesJob>(*_featuresExtractor, trackInfo));
        }
        updateTrackAudioFeatures(dbSession, trackAudioFeatureAssociations, false);
    }
} // namespace lms::scanner
