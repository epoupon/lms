/*
 * Copyright (C) 2024 Emeric Poupon
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

#include "ScanStepAssociateExternalLyrics.hpp"

#include <deque>

#include "core/ILogger.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackLyrics.hpp"

#include "ScanContext.hpp"

namespace lms::scanner
{
    namespace
    {
        struct TrackLyricsAssociation
        {
            db::TrackLyricsId trackLyricsId;
            db::TrackId trackId;
        };
        using TrackLyricsAssociationContainer = std::deque<TrackLyricsAssociation>;

        struct SearchTrackLyricsContext
        {
            db::Session& session;
            db::TrackLyricsId lastRetrievedTrackLyricsId;
            std::size_t processedLyricsCount{};
        };

        db::Track::pointer getMatchingTrack(db::Session& session, const db::TrackLyrics::pointer& lyrics)
        {
            db::Track::pointer matchingTrack;

            auto tryMatch = [&](std::string_view stem) {
                db::Track::FindParameters params;
                assert(lyrics->getDirectory()->getId().isValid());
                assert(!lyrics->getFileStem().empty());

                params.setDirectory(lyrics->getDirectory()->getId());

                db::Track::find(session, params, [&](const db::Track::pointer& track) {
                    if (track->getAbsoluteFilePath().filename().stem() != stem)
                        return;

                    if (matchingTrack)
                        LMS_LOG(DBUPDATER, DEBUG, "External lyrics '" << lyrics->getAbsoluteFilePath() << "' already matched with '" << matchingTrack->getAbsoluteFilePath() << "', replaced by '" << track->getAbsoluteFilePath() << "'");

                    matchingTrack = track;
                });
            };

            // First try with the stem. If it does not match, try again with the parent steam, if it exists, to handle the file.languagecode.lrc case
            tryMatch(lyrics->getFileStem());
            if (!matchingTrack)
            {
                std::filesystem::path stem{ lyrics->getFileStem() };
                if (stem.has_extension())
                    tryMatch(stem.stem().string());
            }

            return matchingTrack;
        }

        bool fetchNextTrackLyricsToUpdate(SearchTrackLyricsContext& searchContext, TrackLyricsAssociationContainer& trackLyricsAssociations)
        {
            constexpr std::size_t readBatchSize{ 100 };

            const db::TrackLyricsId trackLyricsId{ searchContext.lastRetrievedTrackLyricsId };

            {
                auto transaction{ searchContext.session.createReadTransaction() };

                db::TrackLyrics::find(searchContext.session, searchContext.lastRetrievedTrackLyricsId, readBatchSize, [&](const db::TrackLyrics::pointer& trackLyrics) {
                    // Only iterate over external lyrics
                    if (trackLyrics->getAbsoluteFilePath().empty())
                        return;

                    db::Track::pointer track{ getMatchingTrack(searchContext.session, trackLyrics) };
                    if (track != trackLyrics->getTrack())
                    {
                        LMS_LOG(DBUPDATER, DEBUG, "Updating track for external lyrics " << trackLyrics->getAbsoluteFilePath() << ", using " << (track ? track->getAbsoluteFilePath() : "<none>"));
                        trackLyricsAssociations.push_back(TrackLyricsAssociation{ .trackLyricsId = trackLyrics->getId(), .trackId = (track ? track->getId() : db::TrackId{}) });
                    }
                    else if (!track)
                    {
                        LMS_LOG(DBUPDATER, DEBUG, "No track found for external lyrics " << trackLyrics->getAbsoluteFilePath() << "'");
                    }

                    searchContext.processedLyricsCount++;
                });
            }

            return trackLyricsId != searchContext.lastRetrievedTrackLyricsId;
        }

        void updateTrackLyrics(db::Session& session, const TrackLyricsAssociation& trackLyricsAssociation)
        {
            db::TrackLyrics::pointer lyrics{ db::TrackLyrics::find(session, trackLyricsAssociation.trackLyricsId) };
            assert(lyrics);

            db::Track::pointer track;
            if (trackLyricsAssociation.trackId.isValid())
                track = db::Track::find(session, trackLyricsAssociation.trackId);

            lyrics.modify()->setTrack(track);
        }

        void updateTrackLyrics(db::Session& session, TrackLyricsAssociationContainer& lyricsAssociations)
        {
            constexpr std::size_t writeBatchSize{ 20 };

            while (!lyricsAssociations.empty())
            {
                auto transaction{ session.createWriteTransaction() };

                for (std::size_t i{}; !lyricsAssociations.empty() && i < writeBatchSize; ++i)
                {
                    updateTrackLyrics(session, lyricsAssociations.front());
                    lyricsAssociations.pop_front();
                }
            }
        }
    } // namespace

    bool ScanStepAssociateExternalLyrics::needProcess(const ScanContext& context) const
    {
        if (context.stats.getChangesCount() > 0)
            return true;

        return false;
    }

    void ScanStepAssociateExternalLyrics::process(ScanContext& context)
    {
        auto& session{ _db.getTLSSession() };

        {
            auto transaction{ session.createReadTransaction() };
            context.currentStepStats.totalElems = db::TrackLyrics::getExternalLyricsCount(session);
        }

        SearchTrackLyricsContext searchContext{
            .session = session,
            .lastRetrievedTrackLyricsId = {},
        };

        TrackLyricsAssociationContainer trackLyricsAssociations;
        while (fetchNextTrackLyricsToUpdate(searchContext, trackLyricsAssociations))
        {
            if (_abortScan)
                return;

            updateTrackLyrics(session, trackLyricsAssociations);
            context.currentStepStats.processedElems = searchContext.processedLyricsCount;
            _progressCallback(context.currentStepStats);
        }
    }
} // namespace lms::scanner
