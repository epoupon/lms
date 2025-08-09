/*
 * Copyright (C) 2025 Emeric Poupon
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

#include "ScanStepAssociateTrackImages.hpp"

#include <cassert>
#include <deque>
#include <optional>

#include "core/IJob.hpp"
#include "core/IJobScheduler.hpp"
#include "core/ILogger.hpp"
#include "core/LiteralString.hpp"
#include "database/IDb.hpp"
#include "database/IdRange.hpp"
#include "database/Session.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/Image.hpp"
#include "database/objects/Medium.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackEmbeddedImage.hpp"

#include "JobQueue.hpp"
#include "ScanContext.hpp"

namespace lms::scanner
{
    namespace
    {
        // May come from an embedded image in a track, or from what has been previously resolved for the release
        struct TrackArtworksAssociation
        {
            db::TrackId trackId;
            std::optional<db::ArtworkId> preferredArtworkId;
            std::optional<db::ArtworkId> preferredMediaArtworkId;
        };
        using TrackArtworksAssociationContainer = std::deque<TrackArtworksAssociation>;

        db::Artwork::pointer computePreferredTrackArtwork(db::Session& session, const db::Track::pointer& track, const db::Artwork::pointer& preferredMediaArtwork)
        {
            db::Artwork::pointer res{ preferredMediaArtwork };
            if (res)
                return res;

            // Fallback on front cover for this track
            {
                db::TrackEmbeddedImage::FindParameters params;
                params.setTrack(track->getId());
                params.setImageType(db::ImageType::FrontCover);
                params.setSortMethod(db::TrackEmbeddedImageSortMethod::SizeDesc);

                db::TrackEmbeddedImage::find(session, params, [&](const db::TrackEmbeddedImage::pointer& image) {
                    if (!res)
                        res = db::Artwork::find(session, image->getId());
                });
            }

            if (res)
                return res;

            // Fallback on the artwork already resolved for the release
            const db::ReleaseId releaseId{ track->getReleaseId() };
            if (!releaseId.isValid())
                return res;

            if (const db::Release::pointer release{ db::Release::find(session, releaseId) })
                res = release->getPreferredArtwork();

            return res;
        }

        db::Artwork::pointer computePreferredTrackMediaArtwork(db::Session& session, const db::Track::pointer& track)
        {
            db::Artwork::pointer res;

            {
                db::TrackEmbeddedImage::FindParameters params;
                params.setTrack(track->getId());
                params.setImageType(db::ImageType::Media);
                params.setSortMethod(db::TrackEmbeddedImageSortMethod::SizeDesc);

                db::TrackEmbeddedImage::find(session, params, [&](const db::TrackEmbeddedImage::pointer& image) {
                    if (!res)
                        res = db::Artwork::find(session, image->getId());
                });
            }

            if (res)
                return res;

            // fallback on the medium's preferred artwork
            if (const auto medium{ track->getMedium() })
                res = medium->getPreferredArtwork();

            return res;
        }

        void updateTrackPreferredArtworks(db::Session& session, const TrackArtworksAssociation& trackArtworksAssociation)
        {
            assert(trackArtworksAssociation.preferredArtworkId || trackArtworksAssociation.preferredMediaArtworkId);

            if (trackArtworksAssociation.preferredArtworkId)
                db::Track::updatePreferredArtwork(session, trackArtworksAssociation.trackId, *trackArtworksAssociation.preferredArtworkId);

            if (trackArtworksAssociation.preferredMediaArtworkId)
                db::Track::updatePreferredMediaArtwork(session, trackArtworksAssociation.trackId, *trackArtworksAssociation.preferredMediaArtworkId);
        }

        void updateTrackPreferredArtworks(db::Session& session, TrackArtworksAssociationContainer& imageAssociations, bool forceFullBatch)
        {
            constexpr std::size_t writeBatchSize{ 50 };

            while ((forceFullBatch && imageAssociations.size() >= writeBatchSize) || !imageAssociations.empty())
            {
                auto transaction{ session.createWriteTransaction() };

                for (std::size_t i{}; !imageAssociations.empty() && i < writeBatchSize; ++i)
                {
                    updateTrackPreferredArtworks(session, imageAssociations.front());
                    imageAssociations.pop_front();
                }
            }
        }

        bool fetchNextTrackIdRange(db::Session& session, db::TrackId& lastRetrievedTrackId, db::IdRange<db::TrackId>& trackIdRange)
        {
            constexpr std::size_t readBatchSize{ 100 };

            auto transaction{ session.createReadTransaction() };

            trackIdRange = db::Track::findNextIdRange(session, lastRetrievedTrackId, readBatchSize);
            lastRetrievedTrackId = trackIdRange.last;

            return trackIdRange.isValid();
        }

        class ComputeTrackArtworkAssociationsJob : public core::IJob
        {
        public:
            ComputeTrackArtworkAssociationsJob(db::IDb& db, db::IdRange<db::TrackId> trackIdRange)
                : _db{ db }
                , _trackIdRange{ trackIdRange }
            {
            }

            std::span<const TrackArtworksAssociation> getTrackAssociations() const { return _trackAssociations; }
            std::size_t getProcessedTrackCount() const { return _processedTrackCount; }

        private:
            core::LiteralString getName() const override { return "Associate Track Artworks"; }
            void run() override
            {
                auto& session{ _db.getTLSSession() };
                auto transaction{ session.createReadTransaction() };

                db::Track::find(session, _trackIdRange, [&](const db::Track::pointer& track) {
                    const db::Artwork::pointer preferredMediaArtwork{ computePreferredTrackMediaArtwork(session, track) };
                    const db::Artwork::pointer preferredArtwork{ computePreferredTrackArtwork(session, track, preferredMediaArtwork) };

                    TrackArtworksAssociation artworksAssociation{ .trackId = track->getId(), .preferredArtworkId = std::nullopt, .preferredMediaArtworkId = std::nullopt };

                    if (track->getPreferredArtwork() != preferredArtwork)
                    {
                        artworksAssociation.preferredArtworkId = preferredArtwork ? preferredArtwork->getId() : db::ArtworkId{};

                        if (preferredArtwork)
                            LMS_LOG(DBUPDATER, DEBUG, "Updating preferred artwork in track " << track->getAbsoluteFilePath() << " with image in " << preferredArtwork->getAbsoluteFilePath());
                        else
                            LMS_LOG(DBUPDATER, DEBUG, "Removing preferred artwork from track " << track->getAbsoluteFilePath());
                    }

                    if (track->getPreferredMediaArtwork() != preferredMediaArtwork)
                    {
                        artworksAssociation.preferredMediaArtworkId = preferredMediaArtwork ? preferredMediaArtwork->getId() : db::ArtworkId{};

                        if (preferredMediaArtwork)
                            LMS_LOG(DBUPDATER, DEBUG, "Updating preferred media artwork in track " << track->getAbsoluteFilePath() << " with image in " << preferredMediaArtwork->getAbsoluteFilePath());
                        else
                            LMS_LOG(DBUPDATER, DEBUG, "Removing preferred media artwork from track " << track->getAbsoluteFilePath());
                    }

                    if (artworksAssociation.preferredArtworkId || artworksAssociation.preferredMediaArtworkId)
                        _trackAssociations.push_back(artworksAssociation);

                    _processedTrackCount++;
                });
            }

            db::IDb& _db;
            db::IdRange<db::TrackId> _trackIdRange;
            std::vector<TrackArtworksAssociation> _trackAssociations;
            std::size_t _processedTrackCount{};
        };
    } // namespace

    ScanStepAssociateTrackImages::ScanStepAssociateTrackImages(InitParams& initParams)
        : ScanStepBase{ initParams }
    {
    }

    bool ScanStepAssociateTrackImages::needProcess(const ScanContext& context) const
    {
        return context.stats.getChangesCount() > 0;
    }

    void ScanStepAssociateTrackImages::process(ScanContext& context)
    {
        auto& session{ _db.getTLSSession() };

        {
            auto transaction{ session.createReadTransaction() };
            context.currentStepStats.totalElems = db::Track::getCount(session);
        }

        TrackArtworksAssociationContainer trackArtworksAssociations;
        auto processTracks = [&](std::span<std::unique_ptr<core::IJob>> jobs) {
            if (_abortScan)
                return;

            for (const auto& job : jobs)
            {
                const auto& associationJob{ static_cast<const ComputeTrackArtworkAssociationsJob&>(*job) };
                const auto& trackAssociations{ associationJob.getTrackAssociations() };

                trackArtworksAssociations.insert(std::end(trackArtworksAssociations), std::cbegin(trackAssociations), std::cend(trackAssociations));

                context.currentStepStats.processedElems += associationJob.getProcessedTrackCount();
            }

            updateTrackPreferredArtworks(session, trackArtworksAssociations, true);
            _progressCallback(context.currentStepStats);
        };

        {
            JobQueue queue{ getJobScheduler(), 20, processTracks, 1, 0.85F };

            db::TrackId lastRetrievedTrackId;
            db::IdRange<db::TrackId> trackIdRange;
            while (fetchNextTrackIdRange(session, lastRetrievedTrackId, trackIdRange))
                queue.push(std::make_unique<ComputeTrackArtworkAssociationsJob>(_db, trackIdRange));
        }

        // process all remaining associations
        updateTrackPreferredArtworks(session, trackArtworksAssociations, false);
    }
} // namespace lms::scanner
