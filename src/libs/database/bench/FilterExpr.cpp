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

#include "database/IDb.hpp"
#include "database/ISession.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/ArtistFeedback.hpp"
#include "database/objects/Cluster.hpp"
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
#include "database/objects/TrackList.hpp"
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
        constexpr std::size_t clusterTypeCount{ 3 };
        constexpr std::size_t clusterCountPerType{ 20 };
        constexpr std::size_t clusterCountPerTrack{ 2 };
        constexpr std::size_t composerTrackModulo{ 20 }; // a rare link type
        constexpr std::size_t sampleReleaseModulo{ 1'000 };
        constexpr std::size_t prolificTrackModulo{ 10 }; // one artist linked to a tenth of the tracks
        constexpr std::size_t bigTrackListModulo{ 50 };  // one track list with a fiftieth of the tracks
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
            std::vector<ClusterTypeId> clusterTypes;
            // for the queries made for a single entity
            std::vector<ReleaseId> sampleReleases;
            std::vector<TrackId> sampleTracks;
            std::vector<ArtistId> sampleArtists;
            ArtistId prolificArtist;
            TrackListId bigTrackList;
            UserId user;
        };

        template<typename T>
        std::vector<typename T::IdType> pickDistinct(const std::vector<T>& pool, std::size_t count, core::random::PseudoRandomGenerator& rng)
        {
            std::vector<T> picked;
            std::sample(std::cbegin(pool), std::cend(pool), std::back_inserter(picked), count, rng);
            std::vector<typename T::IdType> ids;

            for (const T& entity : picked)
                ids.push_back(entity.getId());

            return ids;
        }

        Wt::WDateTime randomDateTime(core::random::PseudoRandomGenerator& rng)
        {
            std::uniform_int_distribution<int> dayDist{ 0, 10 * 365 };
            std::uniform_int_distribution<int> secondDist{ 0, 24 * 3600 - 1 };
            return Wt::WDateTime{ Wt::WDate{ 2015, 1, 1 }.addDays(dayDist(rng)) }.addSecs(secondDist(rng));
        }

        template<typename FeedbackType, typename ObjectType>
        void star(ISession& session, const ObjectType& object, UserId user, core::random::PseudoRandomGenerator& rng)
        {
            FeedbackType feedback{ FeedbackType::create(session, object, user) };
            feedback.setValue(FeedbackValue::Loved);
            feedback.setDateTime(randomDateTime(rng));
            FeedbackType::update(session, feedback);
        }

        Library generateLibrary()
        {
            Library library;
            library.tmpDb->createTablesAndIndexes();
            const std::unique_ptr<ISession> ownedSession{ library.tmpDb->getDb().createSession() };
            ISession& session{ *ownedSession };

            core::random::PseudoRandomGenerator rng{ 42 };
            std::uniform_int_distribution<int> yearDist{ 1960, 2020 };
            std::uniform_int_distribution<int> listenCountDist{ 1, 5 };
            std::uniform_int_distribution<int> modifiedDaysDist{ 0, 365 };
            std::bernoulli_distribution isCompilation{ compilationRatio };
            std::bernoulli_distribution isListened{ listenedTrackRatio };
            std::bernoulli_distribution isStarred{ starredRatio };

            std::vector<MediaLibrary> mediaLibraries;
            std::vector<Genre> genres;
            std::vector<Mood> moods;
            std::vector<Label> labels;
            std::vector<Country> countries;
            std::vector<Cluster> clusters;
            std::optional<User> user;
            {
                auto transaction{ session.createWriteTransaction() };

                for (std::size_t i{}; i < mediaLibraryCount; ++i)
                    mediaLibraries.push_back(MediaLibrary::create(session, "Library" + std::to_string(i), "/root" + std::to_string(i)));
                for (std::size_t i{}; i < genreCount; ++i)
                    genres.push_back(Genre::create(session, "Genre" + std::to_string(i)));
                for (std::size_t i{}; i < moodCount; ++i)
                    moods.push_back(Mood::create(session, "Mood" + std::to_string(i)));
                for (std::size_t i{}; i < labelCount; ++i)
                    labels.push_back(Label::create(session, "Label" + std::to_string(i)));
                for (std::size_t i{}; i < countryCount; ++i)
                    countries.push_back(Country::create(session, "Country" + std::to_string(i)));
                user = User::create(session, "MyUser");
                for (std::size_t typeIndex{}; typeIndex < clusterTypeCount; ++typeIndex)
                {
                    const ClusterType clusterType{ ClusterType::create(session, "ClusterType" + std::to_string(typeIndex)) };
                    library.clusterTypes.push_back(clusterType.getId());
                    for (std::size_t i{}; i < clusterCountPerType; ++i)
                        clusters.push_back(Cluster::create(session, clusterType.getId(), "Cluster" + std::to_string(typeIndex) + "-" + std::to_string(i)));
                }

                for (const auto& mediaLibrary : mediaLibraries)
                    library.mediaLibraries.push_back(mediaLibrary.getId());
                for (const auto& genre : genres)
                    library.genres.push_back(genre.getId());
                for (const auto& mood : moods)
                    library.moods.push_back(mood.getId());
                for (const auto& label : labels)
                    library.labels.push_back(label.getId());
                for (const auto& country : countries)
                    library.countries.push_back(country.getId());
                library.user = user->getId();
            }

            {
                auto transaction{ session.createWriteTransaction() };

                const Artist prolificArtist{ Artist::create(session, "Prolific") };
                library.prolificArtist = prolificArtist.getId();
                const TrackList bigTrackList{ TrackList::create(session, "BigTrackList", TrackListType::PlayList) };
                library.bigTrackList = bigTrackList.getId();

                for (std::size_t releaseIndex{}; releaseIndex < releaseCount; ++releaseIndex)
                {
                    Release release{ Release::create(session, "Release" + std::to_string(releaseIndex)) };
                    if (releaseIndex % sampleReleaseModulo == 0)
                        library.sampleReleases.push_back(release.getId());
                    const MediumId medium{ Medium::create(session, release.getId()).getId() };
                    const std::optional<MediaLibrary> mediaLibrary{ mediaLibraries[releaseIndex % mediaLibraries.size()] };
                    const bool compilation{ isCompilation(rng) };
                    const std::string releaseArtistName{ compilation ? "Various Artists " + std::to_string(releaseIndex) : "Artist" + std::to_string(releaseIndex) };
                    const Artist releaseArtist{ Artist::create(session, releaseArtistName) };
                    {
                        ReleaseArtistLink link{ ReleaseArtistLink::create(session, release.getId(), releaseArtist.getId(), false) };
                        link.setArtistName(releaseArtistName);
                        ReleaseArtistLink::update(session, link);
                    }
                    release.setCompilation(compilation);
                    Release::update(session, release);
                    // Not drawn from rng, to keep the same random data as before
                    Release::addLabel(session, release.getId(), labels[releaseIndex % labels.size()].getId());
                    Release::addCountry(session, release.getId(), countries[releaseIndex % countries.size()].getId());
                    const int year{ yearDist(rng) };
                    const Wt::WDateTime added{ randomDateTime(rng) };
                    const Wt::WDateTime lastWritten{ added.addDays(modifiedDaysDist(rng)) };

                    if (isStarred(rng))
                        star<ReleaseFeedback>(session, release.getId(), user->getId(), rng);
                    if (isStarred(rng))
                        star<ArtistFeedback>(session, releaseArtist.getId(), user->getId(), rng);

                    for (std::size_t trackIndex{}; trackIndex < trackCountPerRelease; ++trackIndex)
                    {
                        Track track{ Track::create(session) };
                        const TrackId trackId{ track.getId() };
                        track.setName("Track" + std::to_string(releaseIndex) + "-" + std::to_string(trackIndex));
                        track.setRelease(release.getId());
                        track.setMedium(medium);
                        track.setTrackNumber(static_cast<int>(trackIndex));
                        track.setMediaLibrary(mediaLibrary->getId());
                        track.setDate(core::PartialDateTime{ year });
                        track.setDuration(std::chrono::seconds{ 180 });
                        track.setAddedTime(added);
                        track.setLastWriteTime(lastWritten);
                        Track::update(session, track);
                        Track::setGenres(session, trackId, pickDistinct(genres, genreCountPerTrack, rng));
                        Track::setMoods(session, trackId, pickDistinct(moods, moodCountPerTrack, rng));

                        const std::string trackArtistName{ compilation ? "Artist" + std::to_string(releaseIndex) + "-" + std::to_string(trackIndex) : releaseArtistName };
                        const Artist trackArtist{ compilation ? Artist::create(session, trackArtistName) : releaseArtist };
                        {
                            TrackArtistLink link{ TrackArtistLink::create(session, trackId, trackArtist.getId(), TrackArtistLinkType::Artist) };
                            link.setArtistName(trackArtistName);
                            TrackArtistLink::update(session, link);
                        }

                        if (isStarred(rng))
                            star<TrackFeedback>(session, trackId, user->getId(), rng);

                        // Not drawn from rng, to keep the same random data as before
                        const std::size_t trackNumber{ releaseIndex * trackCountPerRelease + trackIndex };
                        std::vector<ClusterId> trackClusters;
                        for (std::size_t i{}; i < clusterCountPerTrack; ++i)
                            trackClusters.push_back(clusters[(trackNumber * 7 + i * 13) % clusters.size()].getId());
                        Track::setClusters(session, trackId, trackClusters);
                        if (trackNumber % composerTrackModulo == 0)
                        {
                            TrackArtistLink link{ TrackArtistLink::create(session, trackId, trackArtist.getId(), TrackArtistLinkType::Composer) };
                            link.setArtistName(trackArtistName);
                            TrackArtistLink::update(session, link);
                        }
                        if (trackNumber % prolificTrackModulo == 0)
                        {
                            TrackArtistLink link{ TrackArtistLink::create(session, trackId, prolificArtist.getId(), TrackArtistLinkType::Artist) };
                            link.setArtistName("Prolific");
                            TrackArtistLink::update(session, link);
                        }
                        if (trackNumber % bigTrackListModulo == 0)
                            TrackListEntry::create(session, trackId, bigTrackList.getId());
                        if (trackIndex == 0 && releaseIndex % sampleReleaseModulo == 0)
                        {
                            library.sampleTracks.push_back(trackId);
                            library.sampleArtists.push_back(releaseArtist.getId());
                        }

                        if (trackNumber % ratedTrackModulo == 0)
                        {
                            RatedTrack ratedTrack{ RatedTrack::create(session, trackId, user->getId()) };
                            ratedTrack.setRating(static_cast<Rating>(1 + static_cast<int>(trackNumber / ratedTrackModulo) % ratingCount));
                            RatedTrack::update(session, ratedTrack);
                        }

                        if (isListened(rng))
                        {
                            const int listenCount{ listenCountDist(rng) };
                            for (int i{}; i < listenCount; ++i)
                                Listen::create(session, user->getId(), trackId, randomDateTime(rng));
                        }
                    }
                }
            }

            // as done periodically in production: without it, the query planner makes poor choices
            library.tmpDb->getDb().analyze();

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
            ListenedAtLeast3,
            LastListenedRecently,
            YearRange,
            Label,
            LabelAndGenre,
            NotLabel,
            Rated,
            RatedAndGenre,
            NotRated,
            Starred,
            StarredAndGenre,
            NotMediaLibrary,
            NotYearRange,
            NotListened,
            NotStarred,
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
            std::pair<core::LiteralString, FilterCase>{ "ListenedAtLeast3", FilterCase::ListenedAtLeast3 },
            std::pair<core::LiteralString, FilterCase>{ "LastListenedRecently", FilterCase::LastListenedRecently },
            std::pair<core::LiteralString, FilterCase>{ "YearRange", FilterCase::YearRange },
            std::pair<core::LiteralString, FilterCase>{ "Label", FilterCase::Label },
            std::pair<core::LiteralString, FilterCase>{ "LabelAndGenre", FilterCase::LabelAndGenre },
            std::pair<core::LiteralString, FilterCase>{ "NotLabel", FilterCase::NotLabel },
            std::pair<core::LiteralString, FilterCase>{ "Rated", FilterCase::Rated },
            std::pair<core::LiteralString, FilterCase>{ "RatedAndGenre", FilterCase::RatedAndGenre },
            std::pair<core::LiteralString, FilterCase>{ "NotRated", FilterCase::NotRated },
            std::pair<core::LiteralString, FilterCase>{ "Starred", FilterCase::Starred },
            std::pair<core::LiteralString, FilterCase>{ "StarredAndGenre", FilterCase::StarredAndGenre },
            std::pair<core::LiteralString, FilterCase>{ "NotMediaLibrary", FilterCase::NotMediaLibrary },
            std::pair<core::LiteralString, FilterCase>{ "NotYearRange", FilterCase::NotYearRange },
            std::pair<core::LiteralString, FilterCase>{ "NotListened", FilterCase::NotListened },
            std::pair<core::LiteralString, FilterCase>{ "NotStarred", FilterCase::NotStarred },
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
            case FilterCase::ListenedAtLeast3:
                return FilterExpr::condition(ListenCountCondition{ FilterOperator::GreaterOrEqual, std::int64_t{ 3 } });
            case FilterCase::LastListenedRecently:
                return FilterExpr::condition(LastListenDateTimeCondition{ FilterOperator::Greater, Wt::WDateTime{ Wt::WDate{ 2022, 1, 1 } } });
            case FilterCase::YearRange:
                return FilterExpr::condition(IntegerCondition{ IntegerField::Year, FilterOperator::Between, FilterRange<std::int64_t>{ 1990, 1999 } });
            case FilterCase::Label:
                return FilterExpr::equal(library.labels.front());
            case FilterCase::LabelAndGenre:
                return FilterExpr::And({ FilterExpr::equal(library.labels.front()), FilterExpr::equal(library.genres.front()) });
            case FilterCase::NotLabel:
                return FilterExpr::Not(FilterExpr::equal(library.labels.front()));
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
            case FilterCase::NotMediaLibrary:
                return FilterExpr::Not(FilterExpr::equal(library.mediaLibraries.front()));
            case FilterCase::NotYearRange:
                return FilterExpr::Not(FilterExpr::condition(IntegerCondition{ IntegerField::Year, FilterOperator::Between, FilterRange<std::int64_t>{ 1990, 1999 } }));
            case FilterCase::NotListened:
                return FilterExpr::Not(FilterExpr::condition(ListenCountCondition{ FilterOperator::Greater, std::int64_t{ 0 } }));
            case FilterCase::NotStarred:
                return FilterExpr::Not(FilterExpr::condition(FeedbackCondition{ FeedbackValue::Loved }));
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
        std::size_t loadFirstPage(ISession& session, const std::vector<Id>& ids)
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
        std::size_t countTracks(ISession& session, const Filter& filter, UserId user)
        {
            Track::FindParameters params;
            setFilter(params, filter, user);
            return Track::getCount(session, params);
        }

        std::size_t countReleases(ISession& session, const Filter& filter, UserId user)
        {
            Release::FindParameters params;
            setFilter(params, filter, user);
            return Release::getCount(session, params);
        }

        std::size_t countArtists(ISession& session, const Filter& filter, UserId user)
        {
            Artist::FindParameters params;
            setFilter(params, filter, user);
            return Artist::getCount(session, params);
        }

        // Queries issued by the UI views (see TrackCollector, ReleaseCollector and ArtistCollector)
        std::size_t viewTracks(ISession& session, ViewMode viewMode, const Filter& filter, UserId user)
        {
            if (viewMode == ViewMode::RecentlyPlayed || viewMode == ViewMode::MostPlayed)
            {
                Listen::StatsFindParameters params;
                setFilter(params, filter, user);
                params.setUser(user).setRange(firstPage);
                std::size_t count{};
                const auto visitor{ [&](const Track&) { ++count; } };
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
            Track::find(session, params, [&](const Track&) { ++count; });
            return count;
        }

        std::size_t viewReleases(ISession& session, ViewMode viewMode, const Filter& filter, UserId user)
        {
            if (viewMode == ViewMode::RecentlyPlayed || viewMode == ViewMode::MostPlayed)
            {
                Listen::StatsFindParameters params;
                setFilter(params, filter, user);
                params.setUser(user).setRange(firstPage);
                std::size_t count{};
                const auto visitor{ [&](const Release&) { ++count; } };
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
            Release::find(session, params, [&](const Release&) { ++count; });
            return count;
        }

        std::size_t viewArtists(ISession& session, ViewMode viewMode, const Filter& filter, UserId user)
        {
            if (viewMode == ViewMode::RecentlyPlayed || viewMode == ViewMode::MostPlayed)
            {
                Listen::ArtistStatsFindParameters params;
                setFilter(params, filter, user);
                params.setUser(user).setRange(firstPage);
                std::size_t count{};
                const auto visitor{ [&](const Artist&) { ++count; } };
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
            Artist::find(session, params, [&](const Artist&) { ++count; });
            return count;
        }

        // Queries filtered by a link type or a cluster type, which hint the query planner (see the "+" in Artist.cpp, Release.cpp and Cluster.cpp)
        std::size_t countArtistsByLinkType(ISession& session, TrackArtistLinkType linkType)
        {
            Artist::FindParameters params;
            params.setTrackArtistLinkType(linkType);
            return Artist::getCount(session, params);
        }

        std::size_t pageArtistsByLinkType(ISession& session, TrackArtistLinkType linkType)
        {
            Artist::FindParameters params;
            params.setTrackArtistLinkType(linkType).setSortMethod(ArtistSortMethod::SortName).setRange(firstPage);

            std::size_t count{};
            Artist::find(session, params, [&](const Artist&) { ++count; });
            return count;
        }

        std::size_t releaseTrackArtists(ISession& session, const Library& library, TrackArtistLinkType linkType)
        {
            std::size_t count{};
            for (const ReleaseId releaseId : library.sampleReleases)
                count += Artist::findIds(session, Artist::FindParameters{}.setRelease(releaseId).setTrackArtistLinkType(linkType)).size();
            return count;
        }

        std::size_t clustersOfReleases(ISession& session, const Library& library)
        {
            std::size_t count{};
            for (const ReleaseId releaseId : library.sampleReleases)
            {
                for (const ClusterTypeId clusterTypeId : library.clusterTypes)
                    count += Cluster::findIds(session, Cluster::FindParameters{}.setRelease(releaseId).setClusterType(clusterTypeId)).size();
            }
            return count;
        }

        std::size_t clustersOfTypes(ISession& session, const Library& library)
        {
            std::size_t count{};
            for (const ClusterTypeId clusterTypeId : library.clusterTypes)
                count += Cluster::findIds(session, Cluster::FindParameters{}.setClusterType(clusterTypeId).setSortMethod(ClusterSortMethod::Name)).size();
            return count;
        }

        // The tag clouds of the pages of a track, a release, an artist and a track list: the most frequent clusters of each type
        std::size_t countTagCloud(ISession& session, const Library& library, const Cluster::FindParameters& scope, ClusterSortMethod sortMethod)
        {
            std::size_t count{};
            for (const ClusterTypeId clusterTypeId : library.clusterTypes)
                count += Cluster::find(session, Cluster::FindParameters{ scope }.setClusterType(clusterTypeId).setSortMethod(sortMethod).setRange(Range{ 0, 3 })).size();
            return count;
        }

        std::size_t tagCloudOfTracks(ISession& session, const Library& library)
        {
            std::size_t count{};
            for (const TrackId trackId : library.sampleTracks)
                count += countTagCloud(session, library, Cluster::FindParameters{}.setTrack(trackId), ClusterSortMethod::Name);
            return count;
        }

        std::size_t tagCloudOfReleases(ISession& session, const Library& library)
        {
            std::size_t count{};
            for (const ReleaseId releaseId : library.sampleReleases)
                count += countTagCloud(session, library, Cluster::FindParameters{}.setRelease(releaseId), ClusterSortMethod::TrackCountDesc);
            return count;
        }

        std::size_t tagCloudOfArtists(ISession& session, const Library& library)
        {
            std::size_t count{};
            for (const ArtistId artistId : library.sampleArtists)
                count += countTagCloud(session, library, Cluster::FindParameters{}.setArtist(artistId), ClusterSortMethod::TrackCountDesc);
            return count;
        }

        std::size_t tagCloudOfProlificArtist(ISession& session, const Library& library)
        {
            return countTagCloud(session, library, Cluster::FindParameters{}.setArtist(library.prolificArtist), ClusterSortMethod::TrackCountDesc);
        }

        std::size_t tagCloudOfBigTrackList(ISession& session, const Library& library)
        {
            return countTagCloud(session, library, Cluster::FindParameters{}.setTrackList(library.bigTrackList), ClusterSortMethod::TrackCountDesc);
        }

        template<typename QueryFunc>
        void runQuery(benchmark::State& state, const Library& library, QueryFunc queryFunc)
        {
            const std::unique_ptr<ISession> ownedSession{ library.tmpDb->getDb().createSession() };
            ISession& session{ *ownedSession };

            std::size_t resultCount{};
            for (auto _ : state)
            {
                auto transaction{ session.createReadTransaction() };
                resultCount = queryFunc(session);
                benchmark::DoNotOptimize(resultCount);
            }
            state.counters["results"] = static_cast<double>(resultCount);
        }

        using CountFunc = std::size_t (*)(ISession&, const Filter&, UserId);
        using ViewFunc = std::size_t (*)(ISession&, ViewMode, const Filter&, UserId);

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
                        runQuery(state, library, [&](ISession& session) { return countFunc(session, filter, library.user); });
                    })->Unit(benchmark::kMicrosecond);
                }
            }

            for (const auto linkType : { TrackArtistLinkType::Artist, TrackArtistLinkType::Composer })
            {
                const std::string linkTypeName{ linkType == TrackArtistLinkType::Artist ? "Artist" : "Composer" };

                benchmark::RegisterBenchmark("LinkType/Artists/Count/" + linkTypeName, [&library, linkType](benchmark::State& state) {
                    runQuery(state, library, [&](ISession& session) { return countArtistsByLinkType(session, linkType); });
                })->Unit(benchmark::kMicrosecond);
                benchmark::RegisterBenchmark("LinkType/Artists/Page/" + linkTypeName, [&library, linkType](benchmark::State& state) {
                    runQuery(state, library, [&](ISession& session) { return pageArtistsByLinkType(session, linkType); });
                })->Unit(benchmark::kMicrosecond);
                benchmark::RegisterBenchmark("LinkType/Release/TrackArtists/" + linkTypeName, [&library, linkType](benchmark::State& state) {
                    runQuery(state, library, [&](ISession& session) { return releaseTrackArtists(session, library, linkType); });
                })->Unit(benchmark::kMicrosecond);
            }
            benchmark::RegisterBenchmark("LinkType/Cluster/OfReleases", [&library](benchmark::State& state) {
                runQuery(state, library, [&](ISession& session) { return clustersOfReleases(session, library); });
            })->Unit(benchmark::kMicrosecond);
            benchmark::RegisterBenchmark("LinkType/Cluster/OfTypes", [&library](benchmark::State& state) {
                runQuery(state, library, [&](ISession& session) { return clustersOfTypes(session, library); });
            })->Unit(benchmark::kMicrosecond);

            using TagCloudFunc = std::size_t (*)(ISession&, const Library&);
            for (const auto& [name, tagCloudFunc] : { std::pair<core::LiteralString, TagCloudFunc>{ "Track", tagCloudOfTracks },
                                                      std::pair<core::LiteralString, TagCloudFunc>{ "Release", tagCloudOfReleases },
                                                      std::pair<core::LiteralString, TagCloudFunc>{ "Artist", tagCloudOfArtists },
                                                      std::pair<core::LiteralString, TagCloudFunc>{ "ProlificArtist", tagCloudOfProlificArtist },
                                                      std::pair<core::LiteralString, TagCloudFunc>{ "TrackList", tagCloudOfBigTrackList } })
            {
                benchmark::RegisterBenchmark("TagCloud/" + std::string{ name.str() }, [&library, tagCloudFunc](benchmark::State& state) {
                    runQuery(state, library, [&](ISession& session) { return tagCloudFunc(session, library); });
                })->Unit(benchmark::kMicrosecond);
            }

            // The listen stats queries join the listens themselves: cost of a filter on the listens on top of it
            constexpr std::array listenViewModes{
                std::pair<core::LiteralString, ViewMode>{ "RecentlyPlayed", ViewMode::RecentlyPlayed },
                std::pair<core::LiteralString, ViewMode>{ "MostPlayed", ViewMode::MostPlayed },
            };
            constexpr std::array listenFilterCases{
                std::pair<core::LiteralString, FilterCase>{ "None", FilterCase::None },
                std::pair<core::LiteralString, FilterCase>{ "Listened", FilterCase::Listened },
                std::pair<core::LiteralString, FilterCase>{ "ListenedAtLeast3", FilterCase::ListenedAtLeast3 },
                std::pair<core::LiteralString, FilterCase>{ "LastListenedRecently", FilterCase::LastListenedRecently },
            };
            for (const auto& [entityName, viewFunc] : viewFuncs)
            {
                for (const auto& [modeName, viewMode] : listenViewModes)
                {
                    for (const auto& [filterName, filterCase] : listenFilterCases)
                    {
                        benchmark::RegisterBenchmark("ViewListen/" + std::string{ entityName.str() } + "/" + std::string{ modeName.str() } + "/" + std::string{ filterName.str() }, [&library, viewFunc, viewMode, filterCase](benchmark::State& state) {
                            const Filter filter{ createFilter(filterCase, library) };
                            runQuery(state, library, [&](ISession& session) { return viewFunc(session, viewMode, filter, library.user); });
                        })->Unit(benchmark::kMicrosecond);
                    }
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
                            runQuery(state, library, [&](ISession& session) { return viewFunc(session, viewMode, filter, library.user); });
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
