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
#include <variant>

#include "core/ILogger.hpp"
#include "database/Artwork.hpp"
#include "database/Db.hpp"
#include "database/Directory.hpp"
#include "database/Image.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/TrackEmbeddedImage.hpp"
#include "database/TrackEmbeddedImageId.hpp"

#include "ArtworkUtils.hpp"
#include "ScanContext.hpp"

namespace lms::scanner
{
    namespace
    {
        // May come from an embedded image in a track, or from what has been previously resolved for the release
        using Artwork = std::variant<std::monostate, db::TrackEmbeddedImageId, db::ArtworkId>;
        bool isSameArtwork(Artwork preferredArtwork, const db::ObjectPtr<db::Artwork>& artwork)
        {
            if (std::holds_alternative<std::monostate>(preferredArtwork))
                return !artwork;

            if (const db::TrackEmbeddedImageId* trackEmbeddedImageId = std::get_if<db::TrackEmbeddedImageId>(&preferredArtwork))
                return artwork && *trackEmbeddedImageId == artwork->getTrackEmbeddedImageId();

            if (const db::ArtworkId* artworkId = std::get_if<db::ArtworkId>(&preferredArtwork))
                return artwork && *artworkId == artwork->getId();

            return false;
        }

        bool isValid(const Artwork& res)
        {
            return !std::holds_alternative<std::monostate>(res);
        }

        struct TrackImageAssociation
        {
            db::Track::pointer track;
            Artwork preferredArtwork;
            Artwork preferredMediaArtwork;
        };
        using TrackImageAssociationContainer = std::deque<TrackImageAssociation>;

        struct SearchTrackImageContext
        {
            db::Session& session;
            db::TrackId lastRetrievedTrackId;
            std::size_t processedTrackCount{};
        };

        Artwork computePreferredTrackArtwork(SearchTrackImageContext& searchContext, const db::Track::pointer& track)
        {
            // Try to get a media image
            Artwork res;
            {
                db::TrackEmbeddedImage::FindParameters params;
                params.setTrack(track->getId());
                params.setImageTypes({ db::ImageType::Media });
                params.setSortMethod(db::TrackEmbeddedImageSortMethod::SizeDesc);
                params.setRange(db::Range{ .offset = 0, .size = 1 });
                db::TrackEmbeddedImage::find(searchContext.session, params, [&](const db::TrackEmbeddedImage::pointer& image) { res = image->getId(); });
            }

            if (isValid(res))
                return res;

            // Fallback on another track of the same disc
            const db::ReleaseId releaseId{ track->getReleaseId() };
            if (!releaseId.isValid())
                return Artwork{};

            {
                db::TrackEmbeddedImage::FindParameters params;
                params.setRelease(releaseId);
                params.setDiscNumber(track->getDiscNumber());
                params.setImageTypes({ db::ImageType::Media });
                params.setSortMethod(db::TrackEmbeddedImageSortMethod::TrackNumberThenSizeDesc);
                params.setRange(db::Range{ .offset = 0, .size = 1 });
                db::TrackEmbeddedImage::find(searchContext.session, params, [&](const db::TrackEmbeddedImage::pointer& image) { res = image->getId(); });
            }

            if (isValid(res))
                return res;

            // Fallback on front cover for this track
            {
                db::TrackEmbeddedImage::FindParameters params;
                params.setTrack(track->getId());
                params.setImageTypes({ db::ImageType::FrontCover });
                params.setSortMethod(db::TrackEmbeddedImageSortMethod::SizeDesc);
                params.setRange(db::Range{ .offset = 0, .size = 1 });
                db::TrackEmbeddedImage::find(searchContext.session, params, [&](const db::TrackEmbeddedImage::pointer& image) { res = image->getId(); });
            }

            if (isValid(res))
                return res;

            // Fallback on the artwork already resolved for the release
            if (const db::Release::pointer release{ db::Release::find(searchContext.session, releaseId) })
            {
                db::ArtworkId artworkId{ release->getPreferredArtworkId() };
                if (artworkId.isValid())
                    res = artworkId;
            }

            return res;
        }

        Artwork computePreferredTrackMediaArtwork(SearchTrackImageContext& searchContext, const db::Track::pointer& track)
        {
            Artwork res;
            {
                db::TrackEmbeddedImage::FindParameters params;
                params.setTrack(track->getId());
                params.setImageTypes({ db::ImageType::Media });
                params.setSortMethod(db::TrackEmbeddedImageSortMethod::SizeDesc);
                params.setRange(db::Range{ .offset = 0, .size = 1 });
                db::TrackEmbeddedImage::find(searchContext.session, params, [&](const db::TrackEmbeddedImage::pointer& image) { res = image->getId(); });
            }

            if (isValid(res))
                return res;

            // fallback on another track of the same disc
            if (const db::ReleaseId releaseId{ track->getReleaseId() }; releaseId.isValid())
            {
                db::TrackEmbeddedImage::FindParameters params;
                params.setRelease(releaseId);
                params.setDiscNumber(track->getDiscNumber());
                params.setImageTypes({ db::ImageType::Media });
                params.setSortMethod(db::TrackEmbeddedImageSortMethod::TrackNumberThenSizeDesc);
                params.setRange(db::Range{ .offset = 0, .size = 1 });
                db::TrackEmbeddedImage::find(searchContext.session, params, [&](const db::TrackEmbeddedImage::pointer& image) { res = image->getId(); });
            }

            return res;
        }

        bool fetchNextTrackArtworksToUpdate(SearchTrackImageContext& searchContext, TrackImageAssociationContainer& trackImageAssociations)
        {
            const db::TrackId trackId{ searchContext.lastRetrievedTrackId };

            {
                constexpr std::size_t readBatchSize{ 100 };

                auto transaction{ searchContext.session.createReadTransaction() };

                db::Track::find(searchContext.session, searchContext.lastRetrievedTrackId, readBatchSize, [&](const db::Track::pointer& track) {
                    const Artwork preferredArtwork{ computePreferredTrackArtwork(searchContext, track) };
                    const Artwork preferredMediaArtwork{ computePreferredTrackMediaArtwork(searchContext, track) };

                    const db::Artwork::pointer currentPreferredArtwork{ track->getPreferredArtwork() };
                    const db::Artwork::pointer currentPreferredMediaArtwork{ track->getPreferredMediaArtwork() };

                    if (!isSameArtwork(preferredArtwork, currentPreferredArtwork)
                        || !isSameArtwork(preferredMediaArtwork, currentPreferredMediaArtwork))
                    {
                        trackImageAssociations.push_back(TrackImageAssociation{ track, preferredArtwork, preferredMediaArtwork });
                    }

                    searchContext.processedTrackCount++;
                });
            }

            return trackId != searchContext.lastRetrievedTrackId;
        }

        void updateTrackPreferredArtwork(db::Session& session, db::Track::pointer& track, Artwork preferredArtwork)
        {
            db::Artwork::pointer artwork;
            if (const db::TrackEmbeddedImageId * trackEmbeddedImageId{ std::get_if<db::TrackEmbeddedImageId>(&preferredArtwork) })
                artwork = utils::getOrCreateArtworkFromTrackEmbeddedImage(session, *trackEmbeddedImageId);
            else if (const db::ArtworkId * artworkId{ std::get_if<db::ArtworkId>(&preferredArtwork) })
                artwork = db::Artwork::find(session, *artworkId);

            // Using track.modify() is quite CPU intensive as the track class has too many fields
            db::Track::updatePreferredArtwork(session, track->getId(), artwork->getId());
            if (artwork)
                LMS_LOG(DBUPDATER, DEBUG, "Updated preferred artwork in track " << track->getAbsoluteFilePath() << " with image in " << utils::toPath(session, artwork->getId()));
            else
                LMS_LOG(DBUPDATER, DEBUG, "Removed preferred artwork from track " << track->getAbsoluteFilePath());
        }

        void updateTrackPreferredMediaArtwork(db::Session& session, db::Track::pointer& track, Artwork preferredArtwork)
        {
            db::Artwork::pointer artwork;
            if (const db::TrackEmbeddedImageId * trackEmbeddedImageId{ std::get_if<db::TrackEmbeddedImageId>(&preferredArtwork) })
                artwork = utils::getOrCreateArtworkFromTrackEmbeddedImage(session, *trackEmbeddedImageId);
            else if (const db::ArtworkId * artworkId{ std::get_if<db::ArtworkId>(&preferredArtwork) })
                artwork = db::Artwork::find(session, *artworkId);

            // Using track.modify() is quite CPU intensive as the track class has too many fields
            db::Track::updatePreferredMediaArtwork(session, track->getId(), artwork->getId());
            if (artwork)
                LMS_LOG(DBUPDATER, DEBUG, "Updated preferred media artwork in track '" << track->getAbsoluteFilePath() << "' with image in " << utils::toPath(session, artwork->getId()));
            else
                LMS_LOG(DBUPDATER, DEBUG, "Removed preferred media artwork from track '" << track->getAbsoluteFilePath() << "'");
        }

        void updateTrackPreferredArtworks(db::Session& session, const TrackImageAssociation& trackImageAssociation)
        {
            db::Track::pointer track{ trackImageAssociation.track };

            {
                const db::Artwork::pointer currentPreferredArtwork{ track->getPreferredArtwork() };
                if (!isSameArtwork(trackImageAssociation.preferredArtwork, currentPreferredArtwork))
                    updateTrackPreferredArtwork(session, track, trackImageAssociation.preferredArtwork);
            }

            {
                const db::Artwork::pointer currentPreferredMediaArtwork{ track->getPreferredMediaArtwork() };
                if (!isSameArtwork(trackImageAssociation.preferredMediaArtwork, currentPreferredMediaArtwork))
                    updateTrackPreferredMediaArtwork(session, track, trackImageAssociation.preferredMediaArtwork);
            }
        }

        void updateTrackPreferredArtworks(db::Session& session, TrackImageAssociationContainer& imageAssociations)
        {
            constexpr std::size_t writeBatchSize{ 50 };

            while (!imageAssociations.empty())
            {
                auto transaction{ session.createWriteTransaction() };

                for (std::size_t i{}; !imageAssociations.empty() && i < writeBatchSize; ++i)
                {
                    updateTrackPreferredArtworks(session, imageAssociations.front());
                    imageAssociations.pop_front();
                }
            }
        }
    } // namespace

    ScanStepAssociateTrackImages::ScanStepAssociateTrackImages(InitParams& initParams)
        : ScanStepBase{ initParams }
    {
    }

    bool ScanStepAssociateTrackImages::needProcess(const ScanContext& context) const
    {
        return context.stats.nbChanges() > 0;
    }

    void ScanStepAssociateTrackImages::process(ScanContext& context)
    {
        auto& session{ _db.getTLSSession() };

        {
            auto transaction{ session.createReadTransaction() };
            context.currentStepStats.totalElems = db::Track::getCount(session);
        }

        SearchTrackImageContext searchContext{
            .session = session,
            .lastRetrievedTrackId = {},
        };

        TrackImageAssociationContainer trackImageAssociations;
        while (fetchNextTrackArtworksToUpdate(searchContext, trackImageAssociations))
        {
            if (_abortScan)
                return;

            updateTrackPreferredArtworks(session, trackImageAssociations);
            context.currentStepStats.processedElems = searchContext.processedTrackCount;
            _progressCallback(context.currentStepStats);
        }
    }
} // namespace lms::scanner
