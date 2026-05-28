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

#include "ScanStepExtractMusicNNEmbeddings.hpp"

#include <deque>
#include <optional>

#include "core/IJob.hpp"
#include "core/IJobScheduler.hpp"
#include "core/ILogger.hpp"

#include "audio/Exception.hpp"
#include "audio/IMusicNNEmbeddingExtractor.hpp"
#include "audio/MusicNNEmbeddings.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/ScanSettings.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackMusicNNEmbeddings.hpp"
#include "services/scanner/ScanErrors.hpp"

#include "JobQueue.hpp"
#include "ScanContext.hpp"
#include "ScannerSettings.hpp"
#include "TrackLocation.hpp"

namespace lms::scanner
{
    namespace
    {
        struct TrackEmbeddingAssociation
        {
            db::TrackId trackId;
            std::optional<audio::TrackMusicNNEmbeddings> embeddings;
        };
        using TrackEmbeddingAssociationContainer = std::deque<TrackEmbeddingAssociation>;

        db::Track::FindParameters createFindTrackParams(db::TrackId lastRetrievedTrackId = {})
        {
            db::Track::FindParameters params;
            params.setHasMusicNNEmbeddings(false);
            params.setSortMethod(db::TrackSortMethod::Id);
            params.setLastTrackId(lastRetrievedTrackId);
            params.setRange(db::Range{ .offset = 0, .size = 1 });

            return params;
        }

        bool fetchNextTrackWithoutEmbeddings(db::Session& session, db::TrackId& lastRetrievedTrackId, TrackLocation& trackLocation)
        {
            auto transaction{ session.createReadTransaction() };

            const db::Track::FindParameters params{ createFindTrackParams(lastRetrievedTrackId) };

            trackLocation.track = db::TrackId{};
            trackLocation.trackPath.clear();
            db::Track::findAbsoluteFilePath(session, params, [&](db::TrackId trackId, const std::filesystem::path& absoluteFilePath) {
                trackLocation.track = trackId;
                trackLocation.trackPath = absoluteFilePath;
            });
            lastRetrievedTrackId = trackLocation.track;
            return trackLocation.track.isValid();
        }

        class ExtractMusicNNEmbeddingsJob : public core::IJob
        {
        public:
            ExtractMusicNNEmbeddingsJob(const audio::IMusicNNEmbeddingExtractor& extractor, const TrackLocation& trackLocation)
                : _extractor{ extractor }
                , _trackLocation{ trackLocation }
            {
            }
            ~ExtractMusicNNEmbeddingsJob() override = default;
            ExtractMusicNNEmbeddingsJob(const ExtractMusicNNEmbeddingsJob&) = delete;
            ExtractMusicNNEmbeddingsJob& operator=(const ExtractMusicNNEmbeddingsJob&) = delete;

            const TrackLocation& getTrackLocation() const { return _trackLocation; }
            const audio::TrackMusicNNEmbeddings* getEmbeddings() const { return _embeddings ? &_embeddings.value() : nullptr; }
            std::string_view getErrorMessage() const { return _errorMessage; }

        private:
            core::LiteralString getName() const override { return "Extract MusicNN Embeddings"; }

            void run() override
            {
                try
                {
                    LMS_LOG(DBUPDATER, DEBUG, "Extracting MusicNN embeddings for " << _trackLocation.trackPath);
                    const auto result{ _extractor.extract(_trackLocation.trackPath) };
                    if (result.patchCount > 0)
                        _embeddings.emplace(result.embeddings);
                    LMS_LOG(DBUPDATER, DEBUG, "MusicNN extraction complete for " << _trackLocation.trackPath << " (" << result.patchCount << " patches)");
                }
                catch (const audio::Exception& e)
                {
                    _errorMessage = e.what();
                }
            }

            const audio::IMusicNNEmbeddingExtractor& _extractor;
            const TrackLocation _trackLocation;
            std::optional<audio::TrackMusicNNEmbeddings> _embeddings;
            std::string _errorMessage;
        };

        void writeEmbedding(db::Session& session, const TrackEmbeddingAssociation& assoc)
        {
            db::Track::pointer track{ db::Track::find(session, assoc.trackId) };
            assert(track);

            std::vector<std::byte> blob(sizeof(audio::TrackMusicNNEmbeddings));
            audio::trackMusicNNEmbeddingsToBlob(*assoc.embeddings, blob);
            db::TrackMusicNNEmbeddings::pointer entry{ session.create<db::TrackMusicNNEmbeddings>(track) };
            entry.modify()->setData(blob);
        }

        void writeEmbeddings(ScanContext& context, db::Session& session, TrackEmbeddingAssociationContainer& pendingAssocs, bool forceFullBatch)
        {
            constexpr std::size_t writeBatchSize{ 10 };

            while ((forceFullBatch && pendingAssocs.size() >= writeBatchSize) || (!forceFullBatch && !pendingAssocs.empty()))
            {
                auto transaction{ session.createWriteTransaction() };

                for (std::size_t i{}; !pendingAssocs.empty() && i < writeBatchSize; ++i)
                {
                    writeEmbedding(session, pendingAssocs.front());
                    pendingAssocs.pop_front();
                    context.stats.featureExtractions += 1;
                }
            }
        }
    } // namespace

    ScanStepExtractMusicNNEmbeddings::ScanStepExtractMusicNNEmbeddings(InitParams& initParams, const std::filesystem::path& modelPath, std::size_t musicnnMaxPatchCountPerTrack)
        : ScanStepBase{ initParams }
        , _embeddingExtractor{ audio::createMusicNNEmbeddingExtractor(modelPath, musicnnMaxPatchCountPerTrack) }
    {
    }

    ScanStepExtractMusicNNEmbeddings::~ScanStepExtractMusicNNEmbeddings() = default;

    bool ScanStepExtractMusicNNEmbeddings::needProcess([[maybe_unused]] const ScanContext& context) const
    {
        return true;
    }

    void ScanStepExtractMusicNNEmbeddings::process(ScanContext& context)
    {
        db::Session& dbSession{ _db.getTLSSession() };

        {
            const std::string fileIdentifier{ audio::getMusicNNModelIdentifier(_settings.musicnnModelPath) };
            if (fileIdentifier.empty())
            {
                LMS_LOG(DBUPDATER, WARNING, "Cannot identify MusicNN model file, skipping embedding extraction");
                return;
            }
            const std::string identifier{ fileIdentifier + "|" + std::to_string(_settings.musicnnMaxPatchCountPerTrack) };

            auto transaction{ dbSession.createWriteTransaction() };
            db::ScanSettings::pointer settings{ db::ScanSettings::find(dbSession) };
            assert(settings);
            if (settings->getMusicNNModelIdentifier() != identifier)
            {
                LMS_LOG(DBUPDATER, INFO, "MusicNN model changed, clearing embeddings");
                db::TrackMusicNNEmbeddings::removeAll(dbSession);
                settings.modify()->setMusicNNModelIdentifier(identifier);
            }
        }

        {
            db::Track::FindParameters params{ createFindTrackParams() };
            auto transaction{ dbSession.createReadTransaction() };
            context.currentStepStats.totalElems = db::Track::getCount(dbSession, params);
        }

        TrackEmbeddingAssociationContainer pendingAssocs;

        auto processResults{ [&](std::span<std::unique_ptr<core::IJob>> jobs) {
            if (_abortScan)
                return;

            for (const auto& job : jobs)
            {
                const auto& extractJob{ static_cast<const ExtractMusicNNEmbeddingsJob&>(*job) };

                if (const audio::TrackMusicNNEmbeddings * embeddings{ extractJob.getEmbeddings() })
                    pendingAssocs.push_back(TrackEmbeddingAssociation{ .trackId = extractJob.getTrackLocation().track, .embeddings = *embeddings });
                else
                    addError<MusicNNEmbeddingsExtractError>(context, extractJob.getTrackLocation().trackPath, extractJob.getErrorMessage());
            }

            context.currentStepStats.processedElems += jobs.size();
            writeEmbeddings(context, dbSession, pendingAssocs, true);
            _progressCallback(context.currentStepStats);
        } };

        {
            JobQueue queue{ getJobScheduler(), 50, processResults, 1, 0.85F };

            db::TrackId lastRetrievedTrackId;
            TrackLocation trackLocation;
            while (!_abortScan && fetchNextTrackWithoutEmbeddings(dbSession, lastRetrievedTrackId, trackLocation))
                queue.push(std::make_unique<ExtractMusicNNEmbeddingsJob>(*_embeddingExtractor, trackLocation));
        }

        writeEmbeddings(context, dbSession, pendingAssocs, false);
    }
} // namespace lms::scanner
