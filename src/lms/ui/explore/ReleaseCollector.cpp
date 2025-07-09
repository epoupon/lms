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

#include "ReleaseCollector.hpp"

#include "core/Service.hpp"
#include "database/Session.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/User.hpp"
#include "services/feedback/IFeedbackService.hpp"
#include "services/scrobbling/IScrobblingService.hpp"

#include "Filters.hpp"
#include "LmsApplication.hpp"

namespace lms::ui
{
    db::RangeResults<db::ReleaseId> ReleaseCollector::get(std::optional<db::Range> requestedRange)
    {
        feedback::IFeedbackService& feedbackService{ *core::Service<feedback::IFeedbackService>::get() };
        scrobbling::IScrobblingService& scrobblingService{ *core::Service<scrobbling::IScrobblingService>::get() };

        const db::Range range{ getActualRange(requestedRange) };

        db::RangeResults<db::ReleaseId> releases;

        switch (getMode())
        {
        case Mode::Random:
            releases = getRandomReleases(range);
            break;

        case Mode::Starred:
            {
                feedback::IFeedbackService::FindParameters params;
                params.setUser(LmsApp->getUserId());
                params.setFilters(getDbFilters());
                params.setKeywords(getSearchKeywords());
                params.setRange(range);
                releases = feedbackService.findStarredReleases(params);
                break;
            }

        case ReleaseCollector::Mode::RecentlyPlayed:
            {
                scrobbling::IScrobblingService::FindParameters params;
                params.setUser(LmsApp->getUserId());
                params.setFilters(getDbFilters());
                params.setKeywords(getSearchKeywords());
                params.setRange(range);

                releases = scrobblingService.getRecentReleases(params);
                break;
            }

        case Mode::MostPlayed:
            {
                scrobbling::IScrobblingService::FindParameters params;
                params.setUser(LmsApp->getUserId());
                params.setFilters(getDbFilters());
                params.setKeywords(getSearchKeywords());
                params.setRange(range);

                releases = scrobblingService.getTopReleases(params);
                break;
            }

        case Mode::RecentlyAdded:
            {
                db::Release::FindParameters params;
                params.setFilters(getDbFilters());
                params.setKeywords(getSearchKeywords());
                params.setSortMethod(db::ReleaseSortMethod::AddedDesc);
                params.setRange(range);

                {
                    auto transaction{ LmsApp->getDbSession().createReadTransaction() };
                    releases = db::Release::findIds(LmsApp->getDbSession(), params);
                }
                break;
            }

        case Mode::RecentlyModified:
            {
                db::Release::FindParameters params;
                params.setFilters(getDbFilters());
                params.setKeywords(getSearchKeywords());
                params.setSortMethod(db::ReleaseSortMethod::LastWrittenDesc);
                params.setRange(range);

                {
                    auto transaction{ LmsApp->getDbSession().createReadTransaction() };
                    releases = db::Release::findIds(LmsApp->getDbSession(), params);
                }
                break;
            }

        case Mode::All:
            {
                db::Release::FindParameters params;
                params.setFilters(getDbFilters());
                params.setSortMethod(db::ReleaseSortMethod::SortName);
                params.setKeywords(getSearchKeywords());
                params.setRange(range);

                {
                    auto transaction{ LmsApp->getDbSession().createReadTransaction() };
                    releases = db::Release::findIds(LmsApp->getDbSession(), params);
                }
                break;
            }
        }

        if (range.offset + range.size == getMaxCount())
            releases.moreResults = false;

        return releases;
    }

    db::RangeResults<db::ReleaseId> ReleaseCollector::getRandomReleases(Range range)
    {
        assert(getMode() == Mode::Random);

        if (!_randomReleases)
        {
            db::Release::FindParameters params;
            params.setFilters(getDbFilters());
            params.setKeywords(getSearchKeywords());
            params.setSortMethod(db::ReleaseSortMethod::Random);
            params.setRange(db::Range{ 0, getMaxCount() });

            {
                auto transaction{ LmsApp->getDbSession().createReadTransaction() };
                _randomReleases = db::Release::findIds(LmsApp->getDbSession(), params);
            }
        }

        return _randomReleases->getSubRange(range);
    }

} // namespace lms::ui
