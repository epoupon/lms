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
#include <database/Artist.hpp>
#include <database/Release.hpp>

#include "database/Session.hpp"
#include "database/Track.hpp"
#include "services/scrobbling/IScrobblingService.hpp"
#include "Filters.hpp"
#include "LmsApplication.hpp"

namespace lms::ui
{
    using namespace db;

    // TODO: calculate relevance
    // TODO: restore filter by star
    RangeResults<MediumId> MultisearchCollector::get(const std::optional<Range>& requestedRange) const
    {
        auto tracks = getTracks(requestedRange);
        auto releases = getReleases(requestedRange);
        auto artists = getArtists(requestedRange);

        std::vector<MediumId> results(tracks.results.size() + artists.results.size() + releases.results.size());
        for (auto e: tracks.results)
            results.emplace_back(e);
        for (auto e: releases.results)
            results.emplace_back(e);
        for (auto e: artists.results)
            results.emplace_back(e);

        return {
            Range{},
            results,
            tracks.moreResults || releases.moreResults || artists.moreResults
        };
    }

    db::RangeResults<db::TrackId> MultisearchCollector::getTracks(const std::optional<db::Range> &requestedRange) const {
        const Range range{ getActualRange(requestedRange) };

        RangeResults<TrackId> results;

        Track::FindParameters params;
        params.setClusters(getFilters().getClusters());
        params.setMediaLibrary(getFilters().getMediaLibrary());
        params.setKeywords(getSearchKeywords());
        params.setRange(range);

        {
            auto transaction = LmsApp->getDbSession().createReadTransaction();
            results = Track::findIds(LmsApp->getDbSession(), params);
        }

        if (range.offset + range.size >= getMaxCount())
            results.moreResults = false;

        return results;

    }

    db::RangeResults<db::ReleaseId> MultisearchCollector::getReleases(const std::optional<db::Range> &requestedRange) const {
        const Range range{ getActualRange(requestedRange) };

        RangeResults<ReleaseId> results;

        Release::FindParameters params;
        params.setClusters(getFilters().getClusters());
        params.setMediaLibrary(getFilters().getMediaLibrary());
        params.setKeywords(getSearchKeywords());
        params.setRange(range);

        {
            auto transaction = LmsApp->getDbSession().createReadTransaction();
            results = Release::findIds(LmsApp->getDbSession(), params);
        }

        if (range.offset + range.size >= getMaxCount())
            results.moreResults = false;

        return results;

    }

    db::RangeResults<db::ArtistId> MultisearchCollector::getArtists(const std::optional<db::Range> &requestedRange) const {
        const Range range{ getActualRange(requestedRange) };

        RangeResults<ArtistId> results;

        Artist::FindParameters params;
        params.setClusters(getFilters().getClusters());
        params.setMediaLibrary(getFilters().getMediaLibrary());
        params.setKeywords(getSearchKeywords());
        params.setRange(range);

        {
            auto transaction = LmsApp->getDbSession().createReadTransaction();
            results = Artist::findIds(LmsApp->getDbSession(), params);
        }

        if (range.offset + range.size >= getMaxCount())
            results.moreResults = false;

        return results;

    }
} // ns UserInterface
