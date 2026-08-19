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

#include "database/objects/Listen.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackList.hpp"

#include "LmsApplication.hpp"

namespace lms::ui
{
    void TrackCollector::get(db::Range requestedRange, bool& moreResults, const std::function<void(const db::ObjectPtr<db::Track>&)>& func)
    {
        const db::Range range{ getActualRange(requestedRange) };

        switch (getMode())
        {
        case Mode::Random:
            {
                if (!_randomTracks)
                {
                    db::Track::FindParameters params;
                    params.setFilters(getDbFilters());
                    params.setKeywords(getSearchKeywords());
                    params.setSortMethod(db::TrackSortMethod::Random);
                    params.setRange(db::Range{ 0, getMaxCount() });
                    _randomTracks = db::Track::findIds(LmsApp->getDbSession(), params);
                }
                const auto& all{ *_randomTracks };
                const std::size_t offset{ std::min(range.offset, all.size()) };
                const auto slice{ std::span{ all }.subspan(offset, std::min(range.size, all.size() - offset)) };
                moreResults = (offset + slice.size() < all.size());
                for (const auto id : slice)
                {
                    if (const auto track{ db::Track::find(LmsApp->getDbSession(), id) })
                        func(track);
                }
                break;
            }

        case Mode::Starred:
            {
                db::Track::FindParameters params;
                params.setFilters(getDbFilters());
                params.setKeywords(getSearchKeywords());
                params.setSortMethod(db::TrackSortMethod::StarredDateDesc);
                params.setRange(range);
                params.setStarringUser(LmsApp->getUserId());
                std::size_t count{};
                db::Track::find(LmsApp->getDbSession(), params, [&](const auto& t) { func(t); ++count; });
                moreResults = (count == range.size);
                break;
            }

        case Mode::RecentlyPlayed:
            {
                db::Listen::StatsFindParameters params;
                params.setFilters(getDbFilters());
                params.setKeywords(getSearchKeywords());
                params.setRange(range);
                params.setUser(LmsApp->getUserId());
                std::size_t count{};
                db::Listen::getRecentTracks(LmsApp->getDbSession(), params, [&](const auto& t) { func(t); ++count; });
                moreResults = (count == range.size);
                break;
            }

        case Mode::MostPlayed:
            {
                db::Listen::StatsFindParameters params;
                params.setFilters(getDbFilters());
                params.setKeywords(getSearchKeywords());
                params.setRange(range);
                params.setUser(LmsApp->getUserId());
                std::size_t count{};
                db::Listen::getTopTracks(LmsApp->getDbSession(), params, [&](const auto& t) { func(t); ++count; });
                moreResults = (count == range.size);
                break;
            }

        case Mode::RecentlyAdded:
            {
                db::Track::FindParameters params;
                params.setFilters(getDbFilters());
                params.setKeywords(getSearchKeywords());
                params.setSortMethod(db::TrackSortMethod::AddedDesc);
                params.setRange(range);
                std::size_t count{};
                db::Track::find(LmsApp->getDbSession(), params, [&](const auto& t) { func(t); ++count; });
                moreResults = (count == range.size);
                break;
            }

        case Mode::RecentlyModified:
            {
                db::Track::FindParameters params;
                params.setFilters(getDbFilters());
                params.setKeywords(getSearchKeywords());
                params.setSortMethod(db::TrackSortMethod::LastWrittenDesc);
                params.setRange(range);
                std::size_t count{};
                db::Track::find(LmsApp->getDbSession(), params, [&](const auto& t) { func(t); ++count; });
                moreResults = (count == range.size);
                break;
            }

        case Mode::All:
            {
                db::Track::FindParameters params;
                params.setFilters(getDbFilters());
                params.setKeywords(getSearchKeywords());
                params.setRange(range);
                std::size_t count{};
                db::Track::find(LmsApp->getDbSession(), params, [&](const auto& t) { func(t); ++count; });
                moreResults = (count == range.size);
                break;
            }
        }
    }

} // namespace lms::ui
