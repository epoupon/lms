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
#include <optional>

#include "core/IJob.hpp"
#include "core/IJobScheduler.hpp"
#include "core/ILogger.hpp"

#include "audio/AudioFeatures.hpp"
#include "audio/Exception.hpp"
#include "audio/IAudioFeaturesExtractor.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackAudioFeatures.hpp"

#include "JobQueue.hpp"
#include "ScanContext.hpp"
#include "TrackLocation.hpp"

namespace lms::scanner
{
    namespace
    {
        struct TrackAudioFeatureAssociation
        {
            db::TrackId trackId;
            std::optional<audio::TrackAudioFeatures> trackFeatures;
        };
        using TrackAudioFeatureAssociationContainer = std::deque<TrackAudioFeatureAssociation>;

        db::Track::FindParameters createFindTrackFeaturesParameters(db::TrackId lastRetrievedTrackId = {})
        {
            db::Track::FindParameters params;
            params.setHasAudioFeatures(false);
            params.setSortMethod(db::TrackSortMethod::Id);
            params.setLastTrackId(lastRetrievedTrackId);
            params.setRange(db::Range{ .offset = 0, .size = 1 });

            return params;
        }

        bool fetchNextTrackWithoutFeatures(db::Session& session, db::TrackId& lastRetrievedTrackId, TrackLocation& trackLocation)
        {
            auto transaction{ session.createReadTransaction() };

            const db::Track::FindParameters params{ createFindTrackFeaturesParameters(lastRetrievedTrackId) };

            trackLocation.track = db::TrackId{};
            trackLocation.trackPath.clear();
            db::Track::findAbsoluteFilePath(session, params, [&](db::TrackId trackId, const std::filesystem::path& absoluteFilePath) {
                trackLocation.track = trackId;
                trackLocation.trackPath = absoluteFilePath;
            });

            lastRetrievedTrackId = trackLocation.track;
            return trackLocation.track.isValid();
        }

        class ExtractAudioFeaturesJob : public core::IJob
        {
        public:
            ExtractAudioFeaturesJob(const audio::IAudioFeaturesExtractor& featuresExtractor, const TrackLocation& trackLocation)
                : _featuresExtractor{ featuresExtractor }
                , _trackLocation{ trackLocation }
            {
            }
            ~ExtractAudioFeaturesJob() override = default;
            ExtractAudioFeaturesJob(const ExtractAudioFeaturesJob&) = delete;
            ExtractAudioFeaturesJob& operator=(const ExtractAudioFeaturesJob&) = delete;

            const TrackLocation& getTrackLocation() const { return _trackLocation; }
            const audio::TrackAudioFeatures* getTrackFeatures() const { return _trackFeatures ? &_trackFeatures.value() : nullptr; }
            std::string_view getErrorMessage() const { return _errorMessage; }

        private:
            core::LiteralString getName() const override { return "Extract Audio Features"; }

            void run() override
            {
                // TODO check for abort!!
                try
                {
                    LMS_LOG(DBUPDATER, DEBUG, "Extracting audio features for " << _trackLocation.trackPath);
                    _trackFeatures.emplace(_featuresExtractor.extractFeatures(_trackLocation.trackPath).features);
                    LMS_LOG(DBUPDATER, DEBUG, "Extracting audio features complete for " << _trackLocation.trackPath);
                }
                catch (const audio::Exception& e)
                {
                    _errorMessage = e.what();
                }
            }

            const audio::IAudioFeaturesExtractor& _featuresExtractor;
            const TrackLocation _trackLocation;
            std::optional<audio::TrackAudioFeatures> _trackFeatures;
            std::string _errorMessage;
        };

        void updateTrackAudioFeatures(db::Session& session, const TrackAudioFeatureAssociation& trackAudioFeatureAssociation)
        {
            db::Track::pointer track{ db::Track::find(session, trackAudioFeatureAssociation.trackId) };
            assert(track);

            std::vector<std::byte> blob(sizeof(audio::TrackAudioFeatures));
            audio::trackAudioFeaturesToBlob(*trackAudioFeatureAssociation.trackFeatures, blob);
            db::TrackAudioFeatures::pointer trackFeatures{ session.create<db::TrackAudioFeatures>(track) };
            trackFeatures.modify()->setData(blob);
        }

        void updateTrackAudioFeatures(ScanContext& context, db::Session& session, TrackAudioFeatureAssociationContainer& trackAudioFeatureAssociations, bool forceFullBatch)
        {
            constexpr std::size_t writeBatchSize{ 10 };

            while ((forceFullBatch && trackAudioFeatureAssociations.size() >= writeBatchSize) || (!forceFullBatch && !trackAudioFeatureAssociations.empty()))
            {
                auto transaction{ session.createWriteTransaction() };

                for (std::size_t i{}; !trackAudioFeatureAssociations.empty() && i < writeBatchSize; ++i)
                {
                    updateTrackAudioFeatures(session, trackAudioFeatureAssociations.front());
                    trackAudioFeatureAssociations.pop_front();

                    context.stats.featureExtractions += 1;
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

        auto processResults{ [&](std::span<std::unique_ptr<core::IJob>> jobs) {
            if (_abortScan)
                return;

            for (const auto& job : jobs)
            {
                const auto& extractAudioFeaturesJob{ static_cast<const ExtractAudioFeaturesJob&>(*job) };

                if (const audio::TrackAudioFeatures * trackFeatures{ extractAudioFeaturesJob.getTrackFeatures() })
                    trackAudioFeatureAssociations.push_back(TrackAudioFeatureAssociation{ .trackId = extractAudioFeaturesJob.getTrackLocation().track, .trackFeatures = *trackFeatures });
                else
                    addError<AudioFeaturesExtractError>(context, extractAudioFeaturesJob.getTrackLocation().trackPath, extractAudioFeaturesJob.getErrorMessage());
            }

            context.currentStepStats.processedElems += jobs.size();
            updateTrackAudioFeatures(context, dbSession, trackAudioFeatureAssociations, true);
            _progressCallback(context.currentStepStats);
        } };

        {
            JobQueue queue{ getJobScheduler(), 50, processResults, 1, 0.85F };

            db::TrackId lastRetrievedTrackId;
            TrackLocation trackLocation;
            while (!_abortScan && fetchNextTrackWithoutFeatures(dbSession, lastRetrievedTrackId, trackLocation))
                queue.push(std::make_unique<ExtractAudioFeaturesJob>(*_featuresExtractor, trackLocation));
        }

        updateTrackAudioFeatures(context, dbSession, trackAudioFeatureAssociations, false);
    }
} // namespace lms::scanner
