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

#include "database/Session.hpp"
#include "database/objects/Listen.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/User.hpp"

#include "Filters.hpp"
#include "LmsApplication.hpp"

namespace lms::ui
{
    void ReleaseCollector::get(db::Range requestedRange, bool& moreResults, const std::function<void(const db::ObjectPtr<db::Release>&)>& func)
    {
        const db::Range range{ getActualRange(requestedRange) };

        switch (getMode())
        {
        case Mode::Random:
            {
                if (!_randomReleases)
                {
                    db::Release::FindParameters params;
                    params.setFilters(getDbFilters());
                    params.setKeywords(getSearchKeywords());
                    params.setSortMethod(db::ReleaseSortMethod::Random);
                    params.setRange(db::Range{ 0, getMaxCount() });
                    _randomReleases = db::Release::findIds(LmsApp->getDbSession(), params);
                }
                const auto& all{ *_randomReleases };
                const std::size_t offset{ std::min(range.offset, all.size()) };
                const auto slice{ std::span{ all }.subspan(offset, std::min(range.size, all.size() - offset)) };
                moreResults = (offset + slice.size() < all.size());
                for (const auto id : slice)
                {
                    if (const auto release{ db::Release::find(LmsApp->getDbSession(), id) })
                        func(release);
                }
                break;
            }

        case Mode::Starred:
            {
                db::Release::FindParameters params;
                params.setFilters(getDbFilters());
                params.setKeywords(getSearchKeywords());
                params.setSortMethod(db::ReleaseSortMethod::StarredDateDesc);
                params.setRange(range);
                params.setStarringUser(LmsApp->getUserId());
                std::size_t count{};
                db::Release::find(LmsApp->getDbSession(), params, [&](const auto& r) { func(r); ++count; });
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
                db::Listen::getRecentReleases(LmsApp->getDbSession(), params, [&](const auto& r) { func(r); ++count; });
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
                db::Listen::getTopReleases(LmsApp->getDbSession(), params, [&](const auto& r) { func(r); ++count; });
                moreResults = (count == range.size);
                break;
            }

        case Mode::RecentlyAdded:
            {
                db::Release::FindParameters params;
                params.setFilters(getDbFilters());
                params.setKeywords(getSearchKeywords());
                params.setSortMethod(db::ReleaseSortMethod::AddedDesc);
                params.setRange(range);
                std::size_t count{};
                db::Release::find(LmsApp->getDbSession(), params, [&](const auto& r) { func(r); ++count; });
                moreResults = (count == range.size);
                break;
            }

        case Mode::RecentlyModified:
            {
                db::Release::FindParameters params;
                params.setFilters(getDbFilters());
                params.setKeywords(getSearchKeywords());
                params.setSortMethod(db::ReleaseSortMethod::LastWrittenDesc);
                params.setRange(range);
                std::size_t count{};
                db::Release::find(LmsApp->getDbSession(), params, [&](const auto& r) { func(r); ++count; });
                moreResults = (count == range.size);
                break;
            }

        case Mode::All:
            {
                db::Release::FindParameters params;
                params.setFilters(getDbFilters());
                params.setSortMethod(db::ReleaseSortMethod::SortName);
                params.setKeywords(getSearchKeywords());
                params.setRange(range);
                std::size_t count{};
                db::Release::find(LmsApp->getDbSession(), params, [&](const auto& r) { func(r); ++count; });
                moreResults = (count == range.size);
                break;
            }
        }
    }

} // namespace lms::ui
