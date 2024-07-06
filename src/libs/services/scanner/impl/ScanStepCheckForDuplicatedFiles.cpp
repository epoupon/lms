/*
 * Copyright (C) 2023 Emeric Poupon
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

#include "ScanStepCheckForDuplicatedFiles.hpp"

#include "core/ILogger.hpp"
#include "database/Db.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"

namespace lms::scanner
{
    void ScanStepCheckForDuplicatedFiles::process(ScanContext& context)
    {
        using namespace db;

        if (_abortScan)
            return;

        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        const RangeResults<TrackId> tracks = Track::findIdsTrackMBIDDuplicates(session);
        for (const TrackId trackId : tracks.results)
        {
            if (_abortScan)
                break;

            const Track::pointer track{ Track::find(session, trackId) };
            if (auto trackMBID{ track->getTrackMBID() })
            {
                LMS_LOG(DBUPDATER, INFO, "Found duplicated track MBID [" << trackMBID->getAsString() << "], file: " << track->getAbsoluteFilePath().string() << " - " << track->getName());
                context.stats.duplicates.emplace_back(ScanDuplicate{ track->getId(), DuplicateReason::SameTrackMBID });
                context.currentStepStats.processedElems++;
                _progressCallback(context.currentStepStats);
            }
        }

        LMS_LOG(DBUPDATER, DEBUG, "Found " << context.currentStepStats.processedElems << " duplicated audio files");
    }
} // namespace lms::scanner
