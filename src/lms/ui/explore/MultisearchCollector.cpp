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
