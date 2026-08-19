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

#include "ArtistCollector.hpp"

#include "core/Utils.hpp"

#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Listen.hpp"
#include "database/objects/TrackList.hpp"
#include "database/objects/Types.hpp"
#include "database/objects/User.hpp"

#include "LmsApplication.hpp"

namespace lms::ui
{
    void ArtistCollector::get(db::Range requestedRange, bool& moreResults, const std::function<void(const db::ObjectPtr<db::Artist>&)>& func)
    {
        const Range range{ getActualRange(requestedRange) };

        auto applyArtistType = [&](auto& params) {
            std::visit(core::utils::overloads{
                           [&](AllArtistsTag) {},
                           [&](ReleaseArtistsTag) { params.setReleaseArtistsOnly(true); },
                           [&](db::TrackArtistLinkType trackArtistLinkType) { params.setTrackArtistLinkType(trackArtistLinkType); } },
                       _artistType);
        };

        switch (getMode())
        {
        case Mode::Random:
            {
                if (!_randomArtists)
                {
                    db::Artist::FindParameters params;
                    params.setFilters(getDbFilters());
                    params.setKeywords(getSearchKeywords());
                    params.setSortMethod(db::ArtistSortMethod::Random);
                    params.setRange(db::Range{ 0, getMaxCount() });
                    applyArtistType(params);
                    _randomArtists = db::Artist::findIds(LmsApp->getDbSession(), params);
                }
                const auto& all{ *_randomArtists };
                const std::size_t offset{ std::min(range.offset, all.size()) };
                const auto slice{ std::span{ all }.subspan(offset, std::min(range.size, all.size() - offset)) };
                moreResults = (offset + slice.size() < all.size());
                for (const auto id : slice)
                {
                    if (const auto artist{ db::Artist::find(LmsApp->getDbSession(), id) })
                        func(artist);
                }
                break;
            }

        case Mode::Starred:
            {
                db::Artist::FindParameters params;
                params.setFilters(getDbFilters());
                params.setKeywords(getSearchKeywords());
                params.setSortMethod(db::ArtistSortMethod::StarredDateDesc);
                params.setRange(range);
                params.setStarringUser(LmsApp->getUserId());
                applyArtistType(params);
                std::size_t count{};
                db::Artist::find(LmsApp->getDbSession(), params, [&](const auto& a) { func(a); ++count; });
                moreResults = (count == range.size);
                break;
            }

        case Mode::RecentlyPlayed:
            {
                db::Listen::ArtistStatsFindParameters params;
                params.setFilters(getDbFilters());
                params.setKeywords(getSearchKeywords());
                params.setRange(range);
                params.setUser(LmsApp->getUserId());
                applyArtistType(params);
                std::size_t count{};
                db::Listen::getRecentArtists(LmsApp->getDbSession(), params, [&](const auto& a) { func(a); ++count; });
                moreResults = (count == range.size);
                break;
            }

        case Mode::MostPlayed:
            {
                db::Listen::ArtistStatsFindParameters params;
                params.setFilters(getDbFilters());
                params.setKeywords(getSearchKeywords());
                params.setRange(range);
                params.setUser(LmsApp->getUserId());
                applyArtistType(params);
                std::size_t count{};
                db::Listen::getTopArtists(LmsApp->getDbSession(), params, [&](const auto& a) { func(a); ++count; });
                moreResults = (count == range.size);
                break;
            }

        case Mode::RecentlyAdded:
            {
                db::Artist::FindParameters params;
                params.setFilters(getDbFilters());
                params.setKeywords(getSearchKeywords());
                params.setSortMethod(db::ArtistSortMethod::AddedDesc);
                params.setRange(range);
                applyArtistType(params);
                std::size_t count{};
                db::Artist::find(LmsApp->getDbSession(), params, [&](const auto& a) { func(a); ++count; });
                moreResults = (count == range.size);
                break;
            }

        case Mode::RecentlyModified:
            {
                db::Artist::FindParameters params;
                params.setFilters(getDbFilters());
                params.setKeywords(getSearchKeywords());
                params.setSortMethod(db::ArtistSortMethod::LastWrittenDesc);
                params.setRange(range);
                applyArtistType(params);
                std::size_t count{};
                db::Artist::find(LmsApp->getDbSession(), params, [&](const auto& a) { func(a); ++count; });
                moreResults = (count == range.size);
                break;
            }

        case Mode::All:
            {
                db::Artist::FindParameters params;
                params.setFilters(getDbFilters());
                params.setKeywords(getSearchKeywords());
                params.setSortMethod(db::ArtistSortMethod::SortName);
                params.setRange(range);
                applyArtistType(params);
                std::size_t count{};
                db::Artist::find(LmsApp->getDbSession(), params, [&](const auto& a) { func(a); ++count; });
                moreResults = (count == range.size);
                break;
            }
        }
    }

} // namespace lms::ui
