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

#include "TrackCollector.hpp"

#include "core/Service.hpp"
#include "database/Session.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackList.hpp"
#include "database/objects/User.hpp"
#include "services/feedback/IFeedbackService.hpp"
#include "services/scrobbling/IScrobblingService.hpp"

#include "Filters.hpp"
#include "LmsApplication.hpp"

namespace lms::ui
{
    using namespace db;

    RangeResults<TrackId> TrackCollector::get(std::optional<Range> requestedRange)
    {
        feedback::IFeedbackService& feedbackService{ *core::Service<feedback::IFeedbackService>::get() };
        scrobbling::IScrobblingService& scrobblingService{ *core::Service<scrobbling::IScrobblingService>::get() };

        const Range range{ getActualRange(requestedRange) };

        RangeResults<TrackId> tracks;

        switch (getMode())
        {
        case Mode::Random:
            tracks = getRandomTracks(range);
            break;

        case Mode::Starred:
            {
                feedback::IFeedbackService::FindParameters params;
                params.setFilters(getDbFilters());
                params.setKeywords(getSearchKeywords());
                params.setRange(range);
                params.setUser(LmsApp->getUserId());
                tracks = feedbackService.findStarredTracks(params);
                break;
            }

        case TrackCollector::Mode::RecentlyPlayed:
            {
                scrobbling::IScrobblingService::FindParameters params;
                params.setUser(LmsApp->getUserId());
                params.setFilters(getDbFilters());
                params.setKeywords(getSearchKeywords());
                params.setRange(range);

                tracks = scrobblingService.getRecentTracks(params);
                break;
            }

        case Mode::MostPlayed:
            {
                scrobbling::IScrobblingService::FindParameters params;
                params.setUser(LmsApp->getUserId());
                params.setFilters(getDbFilters());
                params.setKeywords(getSearchKeywords());
                params.setRange(range);

                tracks = scrobblingService.getTopTracks(params);
                break;
            }

        case Mode::RecentlyAdded:
            {
                Track::FindParameters params;
                params.setFilters(getDbFilters());
                params.setKeywords(getSearchKeywords());
                params.setSortMethod(TrackSortMethod::AddedDesc);
                params.setRange(range);

                {
                    auto transaction{ LmsApp->getDbSession().createReadTransaction() };
                    tracks = Track::findIds(LmsApp->getDbSession(), params);
                }
                break;
            }

        case Mode::RecentlyModified:
            {
                Track::FindParameters params;
                params.setFilters(getDbFilters());
                params.setKeywords(getSearchKeywords());
                params.setSortMethod(TrackSortMethod::LastWrittenDesc);
                params.setRange(range);

                {
                    auto transaction{ LmsApp->getDbSession().createReadTransaction() };
                    tracks = Track::findIds(LmsApp->getDbSession(), params);
                }
                break;
            }

        case Mode::All:
            {
                Track::FindParameters params;
                params.setFilters(getDbFilters());
                params.setKeywords(getSearchKeywords());
                params.setRange(range);

                {
                    auto transaction{ LmsApp->getDbSession().createReadTransaction() };
                    tracks = Track::findIds(LmsApp->getDbSession(), params);
                }
                break;
            }
        }

        if (range.offset + range.size == getMaxCount())
            tracks.moreResults = false;

        return tracks;
    }

    RangeResults<TrackId> TrackCollector::getRandomTracks(Range range)
    {
        assert(getMode() == Mode::Random);

        if (!_randomTracks)
        {
            Track::FindParameters params;
            params.setFilters(getDbFilters());
            params.setKeywords(getSearchKeywords());
            params.setSortMethod(TrackSortMethod::Random);
            params.setRange(Range{ 0, getMaxCount() });

            {
                auto transaction{ LmsApp->getDbSession().createReadTransaction() };
                _randomTracks = Track::findIds(LmsApp->getDbSession(), params);
            }
        }

        return _randomTracks->getSubRange(range);
    }

} // namespace lms::ui
