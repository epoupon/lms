/*
 * Copyright (C) 2026 Emeric Poupon
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

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <benchmark/benchmark.h>

#include "core/ILogger.hpp"
#include "core/LiteralString.hpp"
#include "core/Random.hpp"
#include "core/Service.hpp"

#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/ArtistFeedback.hpp"
#include "database/objects/FilterExpr.hpp"
#include "database/objects/Genre.hpp"
#include "database/objects/Listen.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "database/objects/Medium.hpp"
#include "database/objects/Mood.hpp"
#include "database/objects/RatedTrack.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/ReleaseArtistLink.hpp"
#include "database/objects/ReleaseFeedback.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/TrackFeedback.hpp"
#include "database/objects/User.hpp"

#include "TmpDatabase.hpp"

namespace lms::db::benchs
{
    namespace
    {
        constexpr std::size_t releaseCount{ 10'000 };
        constexpr std::size_t trackCountPerRelease{ 10 };
        constexpr std::size_t mediaLibraryCount{ 3 };
        constexpr std::size_t genreCount{ 50 };
        constexpr std::size_t moodCount{ 25 };
        constexpr std::size_t genreCountPerTrack{ 2 };
        constexpr std::size_t moodCountPerTrack{ 2 };
        constexpr std::size_t labelCount{ 20 };
        constexpr std::size_t countryCount{ 10 };
        constexpr double compilationRatio{ 0.1 };
        constexpr double listenedTrackRatio{ 0.2 };
        constexpr double starredRatio{ 0.05 };
        constexpr std::size_t ratedTrackModulo{ 10 };
        constexpr int ratingCount{ 5 };

        // Same as the UI views
        constexpr Range firstPage{ 0, 50 };
        constexpr Range randomRange{ 0, 5'000 };

        struct Library
        {
            std::unique_ptr<tests::TmpDatabase> tmpDb{ std::make_unique<tests::TmpDatabase>() };
            std::vector<MediaLibraryId> mediaLibraries;
            std::vector<GenreId> genres;
            std::vector<MoodId> moods;
            std::vector<LabelId> labels;
            std::vector<CountryId> countries;
            UserId user;
        };

        template<typename T>
        std::vector<ObjectPtr<T>> pickDistinct(const std::vector<ObjectPtr<T>>& pool, std::size_t count, core::random::PseudoRandomGenerator& rng)
        {
            std::vector<ObjectPtr<T>> picked;
            std::sample(std::cbegin(pool), std::cend(pool), std::back_inserter(picked), count, rng);
            return picked;
        }

        Wt::WDateTime randomDateTime(core::random::PseudoRandomGenerator& rng)
        {
            std::uniform_int_distribution<int> dayDist{ 0, 10 * 365 };
            std::uniform_int_distribution<int> secondDist{ 0, 24 * 3600 - 1 };
            return Wt::WDateTime{ Wt::WDate{ 2015, 1, 1 }.addDays(dayDist(rng)) }.addSecs(secondDist(rng));
        }

        template<typename FeedbackType, typename ObjectType>
        void star(Session& session, const ObjectType& object, const User::pointer& user, core::random::PseudoRandomGenerator& rng)
        {
            auto feedback{ session.create<FeedbackType>(object, user) };
            feedback.modify()->setValue(FeedbackValue::Loved);
            feedback.modify()->setDateTime(randomDateTime(rng));
        }

        Library generateLibrary()
        {
            Library library;
            Session session{ library.tmpDb->getDb() };
            session.prepareTablesIfNeeded();
            session.createIndexesIfNeeded();

            core::random::PseudoRandomGenerator rng{ 42 };
            std::uniform_int_distribution<int> yearDist{ 1960, 2020 };
            std::uniform_int_distribution<int> listenCountDist{ 1, 5 };
            std::uniform_int_distribution<int> modifiedDaysDist{ 0, 365 };
            std::bernoulli_distribution isCompilation{ compilationRatio };
            std::bernoulli_distribution isListened{ listenedTrackRatio };
            std::bernoulli_distribution isStarred{ starredRatio };

            std::vector<MediaLibrary::pointer> mediaLibraries;
            std::vector<Genre::pointer> genres;
            std::vector<Mood::pointer> moods;
            std::vector<Label::pointer> labels;
            std::vector<Country::pointer> countries;
            User::pointer user;
            {
                auto transaction{ session.createWriteTransaction() };

                for (std::size_t i{}; i < mediaLibraryCount; ++i)
                    mediaLibraries.push_back(session.create<MediaLibrary>("Library" + std::to_string(i), "/root" + std::to_string(i)));
                for (std::size_t i{}; i < genreCount; ++i)
                    genres.push_back(session.create<Genre>("Genre" + std::to_string(i)));
                for (std::size_t i{}; i < moodCount; ++i)
                    moods.push_back(session.create<Mood>("Mood" + std::to_string(i)));
                for (std::size_t i{}; i < labelCount; ++i)
                    labels.push_back(session.create<Label>("Label" + std::to_string(i)));
                for (std::size_t i{}; i < countryCount; ++i)
                    countries.push_back(session.create<Country>("Country" + std::to_string(i)));
                user = session.create<User>("MyUser");

                for (const auto& mediaLibrary : mediaLibraries)
                    library.mediaLibraries.push_back(mediaLibrary->getId());
                for (const auto& genre : genres)
                    library.genres.push_back(genre->getId());
                for (const auto& mood : moods)
                    library.moods.push_back(mood->getId());
                for (const auto& label : labels)
                    library.labels.push_back(label->getId());
                for (const auto& country : countries)
                    library.countries.push_back(country->getId());
                library.user = user->getId();
            }

            {
                auto transaction{ session.createWriteTransaction() };
                for (std::size_t releaseIndex{}; releaseIndex < releaseCount; ++releaseIndex)
                {
                    Release::pointer release{ session.create<Release>("Release" + std::to_string(releaseIndex)) };
                    const Medium::pointer medium{ session.create<Medium>(release) };
                    const MediaLibrary::pointer mediaLibrary{ mediaLibraries[releaseIndex % mediaLibraries.size()] };
                    const bool compilation{ isCompilation(rng) };
                    const std::string releaseArtistName{ compilation ? "Various Artists " + std::to_string(releaseIndex) : "Artist" + std::to_string(releaseIndex) };
                    const Artist::pointer releaseArtist{ session.create<Artist>(releaseArtistName) };
                    session.create<ReleaseArtistLink>(release, releaseArtist, false).modify()->setArtistName(releaseArtistName);
                    release.modify()->setCompilation(compilation);
                    // Not drawn from rng, to keep the same random data as before
                    release.modify()->addLabel(labels[releaseIndex % labels.size()]);
                    release.modify()->addCountry(countries[releaseIndex % countries.size()]);
                    const int year{ yearDist(rng) };
                    const Wt::WDateTime added{ randomDateTime(rng) };
                    const Wt::WDateTime lastWritten{ added.addDays(modifiedDaysDist(rng)) };

                    if (isStarred(rng))
                        star<ReleaseFeedback>(session, release, user, rng);
                    if (isStarred(rng))
                        star<ArtistFeedback>(session, releaseArtist, user, rng);

                    for (std::size_t trackIndex{}; trackIndex < trackCountPerRelease; ++trackIndex)
                    {
                        Track::pointer track{ session.create<Track>() };
                        track.modify()->setName("Track" + std::to_string(releaseIndex) + "-" + std::to_string(trackIndex));
                        track.modify()->setRelease(release);
                        track.modify()->setMedium(medium);
                        track.modify()->setTrackNumber(static_cast<int>(trackIndex));
                        track.modify()->setMediaLibrary(mediaLibrary);
                        track.modify()->setDate(core::PartialDateTime{ year });
                        track.modify()->setDuration(std::chrono::seconds{ 180 });
                        track.modify()->setAddedTime(added);
                        track.modify()->setLastWriteTime(lastWritten);
                        track.modify()->setGenres(pickDistinct(genres, genreCountPerTrack, rng));
                        track.modify()->setMoods(pickDistinct(moods, moodCountPerTrack, rng));

                        const std::string trackArtistName{ compilation ? "Artist" + std::to_string(releaseIndex) + "-" + std::to_string(trackIndex) : releaseArtistName };
                        const Artist::pointer trackArtist{ compilation ? session.create<Artist>(trackArtistName) : releaseArtist };
                        session.create<TrackArtistLink>(track, trackArtist, TrackArtistLinkType::Artist).modify()->setArtistName(trackArtistName);

                        if (isStarred(rng))
                            star<TrackFeedback>(session, track, user, rng);

                        // Not drawn from rng, to keep the same random data as before
                        const std::size_t trackNumber{ releaseIndex * trackCountPerRelease + trackIndex };
                        if (trackNumber % ratedTrackModulo == 0)
                            session.create<RatedTrack>(track, user).modify()->setRating(static_cast<Rating>(1 + static_cast<int>(trackNumber / ratedTrackModulo) % ratingCount));

                        if (isListened(rng))
                        {
                            const int listenCount{ listenCountDist(rng) };
                            for (int i{}; i < listenCount; ++i)
                                session.create<Listen>(user, track, randomDateTime(rng));
                        }
                    }
                }
            }

            // as done periodically in production: without it, the query planner makes poor choices
            session.fullAnalyze();

            return library;
        }

        using Filter = std::optional<FilterExpr>;

        enum class FilterCase
        {
            None,
            MediaLibrary,
            Genre,
            GenreAndMood,
            MediaLibraryAndGenre,
            GenreOrGenre,
            NotGenre,
            Listened,
            YearRange,
            Label,
            LabelAndGenre,
            NotLabel,
            ReleaseNameStartsWith,
            ReleaseNameAndGenre,
            ArtistNameStartsWith,
            Rated,
            RatedAndGenre,
            NotRated,
            Starred,
            StarredAndGenre,
        };

        constexpr std::array filterCases{
            std::pair<core::LiteralString, FilterCase>{ "None", FilterCase::None },
            std::pair<core::LiteralString, FilterCase>{ "MediaLibrary", FilterCase::MediaLibrary },
            std::pair<core::LiteralString, FilterCase>{ "Genre", FilterCase::Genre },
            std::pair<core::LiteralString, FilterCase>{ "GenreAndMood", FilterCase::GenreAndMood },
            std::pair<core::LiteralString, FilterCase>{ "MediaLibraryAndGenre", FilterCase::MediaLibraryAndGenre },
            std::pair<core::LiteralString, FilterCase>{ "GenreOrGenre", FilterCase::GenreOrGenre },
            std::pair<core::LiteralString, FilterCase>{ "NotGenre", FilterCase::NotGenre },
            std::pair<core::LiteralString, FilterCase>{ "Listened", FilterCase::Listened },
            std::pair<core::LiteralString, FilterCase>{ "YearRange", FilterCase::YearRange },
            std::pair<core::LiteralString, FilterCase>{ "Label", FilterCase::Label },
            std::pair<core::LiteralString, FilterCase>{ "LabelAndGenre", FilterCase::LabelAndGenre },
            std::pair<core::LiteralString, FilterCase>{ "NotLabel", FilterCase::NotLabel },
            std::pair<core::LiteralString, FilterCase>{ "ReleaseNameStartsWith", FilterCase::ReleaseNameStartsWith },
            std::pair<core::LiteralString, FilterCase>{ "ReleaseNameAndGenre", FilterCase::ReleaseNameAndGenre },
            std::pair<core::LiteralString, FilterCase>{ "ArtistNameStartsWith", FilterCase::ArtistNameStartsWith },
            std::pair<core::LiteralString, FilterCase>{ "Rated", FilterCase::Rated },
            std::pair<core::LiteralString, FilterCase>{ "RatedAndGenre", FilterCase::RatedAndGenre },
            std::pair<core::LiteralString, FilterCase>{ "NotRated", FilterCase::NotRated },
            std::pair<core::LiteralString, FilterCase>{ "Starred", FilterCase::Starred },
            std::pair<core::LiteralString, FilterCase>{ "StarredAndGenre", FilterCase::StarredAndGenre },
        };

        Filter createFilter(FilterCase filterCase, const Library& library)
        {
            switch (filterCase)
            {
            case FilterCase::None:
                return std::nullopt;
            case FilterCase::MediaLibrary:
                return FilterExpr::equal(library.mediaLibraries.front());
            case FilterCase::Genre:
                return FilterExpr::equal(library.genres.front());
            case FilterCase::GenreAndMood:
                return FilterExpr::And({ FilterExpr::equal(library.genres.front()), FilterExpr::equal(library.moods.front()) });
            case FilterCase::MediaLibraryAndGenre:
                return FilterExpr::And({ FilterExpr::equal(library.mediaLibraries.front()), FilterExpr::equal(library.genres.front()) });
            case FilterCase::GenreOrGenre:
                return FilterExpr::Or({ FilterExpr::equal(library.genres[0]), FilterExpr::equal(library.genres[1]) });
            case FilterCase::NotGenre:
                return FilterExpr::Not(FilterExpr::equal(library.genres.front()));
            case FilterCase::Listened:
                return FilterExpr::condition(ListenCountCondition{ FilterOperator::Greater, std::int64_t{ 0 } });
            case FilterCase::YearRange:
                return FilterExpr::condition(IntegerCondition{ IntegerField::Year, FilterOperator::Between, FilterRange<std::int64_t>{ 1990, 1999 } });
            case FilterCase::Label:
                return FilterExpr::equal(library.labels.front());
            case FilterCase::LabelAndGenre:
                return FilterExpr::And({ FilterExpr::equal(library.labels.front()), FilterExpr::equal(library.genres.front()) });
            case FilterCase::NotLabel:
                return FilterExpr::Not(FilterExpr::equal(library.labels.front()));
            case FilterCase::ReleaseNameStartsWith:
                return FilterExpr::condition(StringCondition{ StringField::ReleaseName, StringOperator::StartsWith, "Release1" });
            case FilterCase::ReleaseNameAndGenre:
                return FilterExpr::And({ FilterExpr::condition(StringCondition{ StringField::ReleaseName, StringOperator::StartsWith, "Release1" }), FilterExpr::equal(library.genres.front()) });
            case FilterCase::ArtistNameStartsWith:
                return FilterExpr::condition(StringCondition{ StringField::ArtistName, StringOperator::StartsWith, "Artist1" });
            case FilterCase::Rated:
                return FilterExpr::condition(RatingCondition{ FilterOperator::GreaterOrEqual, std::int64_t{ 4 } });
            case FilterCase::RatedAndGenre:
                return FilterExpr::And({ FilterExpr::condition(RatingCondition{ FilterOperator::GreaterOrEqual, std::int64_t{ 4 } }), FilterExpr::equal(library.genres.front()) });
            case FilterCase::NotRated:
                return FilterExpr::Not(FilterExpr::condition(RatingCondition{ FilterOperator::GreaterOrEqual, std::int64_t{ 4 } }));
            case FilterCase::Starred:
                return FilterExpr::condition(FeedbackCondition{ FeedbackValue::Loved });
            case FilterCase::StarredAndGenre:
                return FilterExpr::And({ FilterExpr::condition(FeedbackCondition{ FeedbackValue::Loved }), FilterExpr::equal(library.genres.front()) });
            }
            return std::nullopt;
        }

        template<typename Params>
        void setFilter(Params& params, const Filter& filter, UserId user)
        {
            params.setFilter(filter, user);
        }

        enum class ViewMode
        {
            Random,
            Starred,
            RecentlyPlayed,
            MostPlayed,
            RecentlyAdded,
            RecentlyModified,
            All,
        };

        constexpr std::array viewModes{
            std::pair<core::LiteralString, ViewMode>{ "Random", ViewMode::Random },
            std::pair<core::LiteralString, ViewMode>{ "Starred", ViewMode::Starred },
            std::pair<core::LiteralString, ViewMode>{ "RecentlyPlayed", ViewMode::RecentlyPlayed },
            std::pair<core::LiteralString, ViewMode>{ "MostPlayed", ViewMode::MostPlayed },
            std::pair<core::LiteralString, ViewMode>{ "RecentlyAdded", ViewMode::RecentlyAdded },
            std::pair<core::LiteralString, ViewMode>{ "RecentlyModified", ViewMode::RecentlyModified },
            std::pair<core::LiteralString, ViewMode>{ "All", ViewMode::All },
        };

        // Random views fetch ids, then load the entities of the page
        template<typename Object, typename Id>
        std::size_t loadFirstPage(Session& session, const std::vector<Id>& ids)
        {
            std::size_t count{};
            for (const Id id : std::span{ ids }.first(std::min(ids.size(), firstPage.size)))
            {
                if (Object::find(session, id))
                    ++count;
            }
            return count;
        }

        // Filter cost only: no sort, no range, all the matching entities are counted
        std::size_t countTracks(Session& session, const Filter& filter, UserId user)
        {
            Track::FindParameters params;
            setFilter(params, filter, user);
            return Track::getCount(session, params);
        }

        std::size_t countReleases(Session& session, const Filter& filter, UserId user)
        {
            Release::FindParameters params;
            setFilter(params, filter, user);
            return Release::getCount(session, params);
        }

        std::size_t countArtists(Session& session, const Filter& filter, UserId user)
        {
            Artist::FindParameters params;
            setFilter(params, filter, user);
            return Artist::getCount(session, params);
        }

        // Queries issued by the UI views (see TrackCollector, ReleaseCollector and ArtistCollector)
        std::size_t viewTracks(Session& session, ViewMode viewMode, const Filter& filter, UserId user)
        {
            if (viewMode == ViewMode::RecentlyPlayed || viewMode == ViewMode::MostPlayed)
            {
                Listen::StatsFindParameters params;
                setFilter(params, filter, user);
                params.setUser(user).setRange(firstPage);
                std::size_t count{};
                const auto visitor{ [&](const Track::pointer&) { ++count; } };
                if (viewMode == ViewMode::RecentlyPlayed)
                    Listen::getRecentTracks(session, params, visitor);
                else
                    Listen::getTopTracks(session, params, visitor);
                return count;
            }

            Track::FindParameters params;
            setFilter(params, filter, user);
            params.setRange(firstPage);
            switch (viewMode)
            {
            case ViewMode::Random:
                params.setSortMethod(TrackSortMethod::Random).setRange(randomRange);
                break;
            case ViewMode::Starred:
                params.setSortMethod(TrackSortMethod::FeedbackDateDesc).setFeedbackUser(user).setFeedbackValue(FeedbackValue::Loved);
                break;
            case ViewMode::RecentlyAdded:
                params.setSortMethod(TrackSortMethod::AddedDesc);
                break;
            case ViewMode::RecentlyModified:
                params.setSortMethod(TrackSortMethod::LastWrittenDesc);
                break;
            case ViewMode::All:
            case ViewMode::RecentlyPlayed:
            case ViewMode::MostPlayed:
                break;
            }

            if (viewMode == ViewMode::Random)
                return loadFirstPage<Track>(session, Track::findIds(session, params));

            std::size_t count{};
            Track::find(session, params, [&](const Track::pointer&) { ++count; });
            return count;
        }

        std::size_t viewReleases(Session& session, ViewMode viewMode, const Filter& filter, UserId user)
        {
            if (viewMode == ViewMode::RecentlyPlayed || viewMode == ViewMode::MostPlayed)
            {
                Listen::StatsFindParameters params;
                setFilter(params, filter, user);
                params.setUser(user).setRange(firstPage);
                std::size_t count{};
                const auto visitor{ [&](const Release::pointer&) { ++count; } };
                if (viewMode == ViewMode::RecentlyPlayed)
                    Listen::getRecentReleases(session, params, visitor);
                else
                    Listen::getTopReleases(session, params, visitor);
                return count;
            }

            Release::FindParameters params;
            setFilter(params, filter, user);
            params.setRange(firstPage);
            switch (viewMode)
            {
            case ViewMode::Random:
                params.setSortMethod(ReleaseSortMethod::Random).setRange(randomRange);
                break;
            case ViewMode::Starred:
                params.setSortMethod(ReleaseSortMethod::FeedbackDateDesc).setFeedbackUser(user).setFeedbackValue(FeedbackValue::Loved);
                break;
            case ViewMode::RecentlyAdded:
                params.setSortMethod(ReleaseSortMethod::AddedDesc);
                break;
            case ViewMode::RecentlyModified:
                params.setSortMethod(ReleaseSortMethod::LastWrittenDesc);
                break;
            case ViewMode::All:
                params.setSortMethod(ReleaseSortMethod::SortName);
                break;
            case ViewMode::RecentlyPlayed:
            case ViewMode::MostPlayed:
                break;
            }

            if (viewMode == ViewMode::Random)
                return loadFirstPage<Release>(session, Release::findIds(session, params));

            std::size_t count{};
            Release::find(session, params, [&](const Release::pointer&) { ++count; });
            return count;
        }

        std::size_t viewArtists(Session& session, ViewMode viewMode, const Filter& filter, UserId user)
        {
            if (viewMode == ViewMode::RecentlyPlayed || viewMode == ViewMode::MostPlayed)
            {
                Listen::ArtistStatsFindParameters params;
                setFilter(params, filter, user);
                params.setUser(user).setRange(firstPage);
                std::size_t count{};
                const auto visitor{ [&](const Artist::pointer&) { ++count; } };
                if (viewMode == ViewMode::RecentlyPlayed)
                    Listen::getRecentArtists(session, params, visitor);
                else
                    Listen::getTopArtists(session, params, visitor);
                return count;
            }

            Artist::FindParameters params;
            setFilter(params, filter, user);
            params.setRange(firstPage);
            switch (viewMode)
            {
            case ViewMode::Random:
                params.setSortMethod(ArtistSortMethod::Random).setRange(randomRange);
                break;
            case ViewMode::Starred:
                params.setSortMethod(ArtistSortMethod::FeedbackDateDesc).setFeedbackUser(user).setFeedbackValue(FeedbackValue::Loved);
                break;
            case ViewMode::RecentlyAdded:
                params.setSortMethod(ArtistSortMethod::AddedDesc);
                break;
            case ViewMode::RecentlyModified:
                params.setSortMethod(ArtistSortMethod::LastWrittenDesc);
                break;
            case ViewMode::All:
                params.setSortMethod(ArtistSortMethod::SortName);
                break;
            case ViewMode::RecentlyPlayed:
            case ViewMode::MostPlayed:
                break;
            }

            if (viewMode == ViewMode::Random)
                return loadFirstPage<Artist>(session, Artist::findIds(session, params));

            std::size_t count{};
            Artist::find(session, params, [&](const Artist::pointer&) { ++count; });
            return count;
        }

        template<typename QueryFunc>
        void runQuery(benchmark::State& state, const Library& library, QueryFunc queryFunc)
        {
            Session session{ library.tmpDb->getDb() };

            std::size_t resultCount{};
            for (auto _ : state)
            {
                auto transaction{ session.createReadTransaction() };
                resultCount = queryFunc(session);
                benchmark::DoNotOptimize(resultCount);
            }
            state.counters["results"] = static_cast<double>(resultCount);
        }

        using CountFunc = std::size_t (*)(Session&, const Filter&, UserId);
        using ViewFunc = std::size_t (*)(Session&, ViewMode, const Filter&, UserId);

        void registerBenchmarks(const Library& library)
        {
            constexpr std::array countFuncs{
                std::pair<core::LiteralString, CountFunc>{ "Tracks", countTracks },
                std::pair<core::LiteralString, CountFunc>{ "Releases", countReleases },
                std::pair<core::LiteralString, CountFunc>{ "Artists", countArtists },
            };
            constexpr std::array viewFuncs{
                std::pair<core::LiteralString, ViewFunc>{ "Tracks", viewTracks },
                std::pair<core::LiteralString, ViewFunc>{ "Releases", viewReleases },
                std::pair<core::LiteralString, ViewFunc>{ "Artists", viewArtists },
            };

            for (const auto& [entityName, countFunc] : countFuncs)
            {
                for (const auto& [filterName, filterCase] : filterCases)
                {
                    benchmark::RegisterBenchmark("Filter/" + std::string{ entityName.str() } + "/" + std::string{ filterName.str() }, [&library, countFunc, filterCase](benchmark::State& state) {
                        const Filter filter{ createFilter(filterCase, library) };
                        runQuery(state, library, [&](Session& session) { return countFunc(session, filter, library.user); });
                    })->Unit(benchmark::kMicrosecond);
                }
            }

            // Sort cost alone, then combined with a media library filter as the UI and Subsonic almost always set one
            for (const auto& [groupName, filterCase] : { std::pair<core::LiteralString, FilterCase>{ "View", FilterCase::None }, std::pair<core::LiteralString, FilterCase>{ "ViewInMediaLibrary", FilterCase::MediaLibrary } })
            {
                for (const auto& [entityName, viewFunc] : viewFuncs)
                {
                    for (const auto& [modeName, viewMode] : viewModes)
                    {
                        benchmark::RegisterBenchmark(std::string{ groupName.str() } + "/" + std::string{ entityName.str() } + "/" + std::string{ modeName.str() }, [&library, viewFunc, viewMode, filterCase](benchmark::State& state) {
                            const Filter filter{ createFilter(filterCase, library) };
                            runQuery(state, library, [&](Session& session) { return viewFunc(session, viewMode, filter, library.user); });
                        })->Unit(benchmark::kMicrosecond);
                    }
                }
            }
        }
    } // namespace
} // namespace lms::db::benchs

int main(int argc, char** argv)
{
    lms::core::Service<lms::core::logging::ILogger> logger{ lms::core::logging::createLogger(lms::core::logging::Severity::ERROR) };

    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv))
        return EXIT_FAILURE;

    std::cout << "Generating library..." << std::endl;
    const lms::db::benchs::Library library{ lms::db::benchs::generateLibrary() };
    lms::db::benchs::registerBenchmarks(library);
    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();

    return EXIT_SUCCESS;
}
