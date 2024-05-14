/*
 * Copyright (C) 2021 Emeric Poupon
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

#include "MultisearchCollector.hpp"

#include <algorithm>

#include "database/Session.hpp"
#include "database/Track.hpp"
#include "services/scrobbling/IScrobblingService.hpp"
#include "Filters.hpp"
#include "LmsApplication.hpp"

namespace lms::ui
{
    using namespace db;

    // TODO: search other things
    // TODO: calculate relevance
    // TODO: restore filter by star
    RangeResults<TrackId> MultisearchCollector::get(const std::optional<Range>& requestedRange) const
    {
        const Range range{ getActualRange(requestedRange) };

        RangeResults<TrackId> tracks;

        Track::FindParameters params;
        params.setClusters(getFilters().getClusters());
        params.setMediaLibrary(getFilters().getMediaLibrary());
        params.setKeywords(getSearchKeywords());
        params.setRange(range);

        {
            auto transaction = LmsApp->getDbSession().createReadTransaction();
            tracks = Track::findIds(LmsApp->getDbSession(), params);
        }

        if (range.offset + range.size >= getMaxCount())
            tracks.moreResults = false;

        return tracks;
    }

} // ns UserInterface
