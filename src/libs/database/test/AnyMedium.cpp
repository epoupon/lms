#include <database/AnyMedium.hpp>
#include <utility>

#include "Common.hpp"

namespace lms::db
{
    struct Keyword
    {
        AnyMediumId id;
        int weight{};
        std::string value;
        std::unordered_set<MediaLibraryId> media_library_ids;
        std::unordered_set<ClusterId> cluster_ids;

        [[nodiscard]] auto tie() const
        {
            return std::tie(id, weight, value, media_library_ids, cluster_ids);
        }

        bool operator==(const Keyword &rhs) const
        {
            return tie() == rhs.tie();
        }
    };

    std::ostream &operator<<(std::ostream &os, const Keyword &v)
    {
        os << v.id << ", " << v.weight << ", " << v.value << ", [";

        for (auto &e: v.media_library_ids)
            os << e.getValue() << ", ";

        os << "], [";

        for (auto &e: v.cluster_ids)
            os << e.getValue() << ", ";

        os << "]";

        return os;
    }

    struct KeywordIds
    {
        AnyMediumId id;
        std::string value;

        KeywordIds(AnyMediumId id,
                   std::string value) : id(id), value(std::move(value))
        {}

        explicit KeywordIds(const Keyword &keyword) : id(keyword.id), value(keyword.value)
        {}

        [[nodiscard]] auto tie() const
        {
            return std::tie(id, value);
        }

        bool operator==(const KeywordIds &rhs) const
        {
            return tie() == rhs.tie();
        }
    };
}

template<>
struct std::hash<lms::db::KeywordIds>
{
    std::size_t operator()(const lms::db::KeywordIds &s) const noexcept
    {
        std::size_t seed = std::hash<lms::db::AnyMediumId>{}(s.id);
        seed = seed ^ (std::hash<std::string>{}(s.value) << 1);

        return seed;
    }
};


namespace Wt::Dbo
{
    template<>
    struct query_result_traits<lms::db::Keyword>
    {
        static void getFields(Session &session,
                              std::vector<std::string> *aliases,
                              std::vector<FieldInfo> &result
        )
        {
            query_result_traits<std::string>::getFields(session, aliases, result);
            query_result_traits<dbo_default_traits::IdType>::getFields(session, aliases, result);
            query_result_traits<int>::getFields(session, aliases, result);
            query_result_traits<std::string>::getFields(session, aliases, result);
            query_result_traits<Json::Array>::getFields(session, aliases, result);
            query_result_traits<Json::Array>::getFields(session, aliases, result);
        }

        static lms::db::Keyword load(Session &session,
                                     SqlStatement &statement,
                                     int &column
        )
        {
            auto type = query_result_traits<std::string>::load(session, statement, column);
            auto id = query_result_traits<dbo_default_traits::IdType>::load(session, statement, column);
            auto weight = query_result_traits<int>::load(session, statement, column);
            auto value = query_result_traits<std::string>::load(session, statement, column);
            auto media_library_ids = query_result_traits<Json::Array>::load(session, statement, column);
            auto cluster_ids = query_result_traits<Json::Array>::load(session, statement, column);

            auto media_library_ids_vec = std::unordered_set<lms::db::MediaLibraryId>(media_library_ids.size());
            for (auto &v: media_library_ids)
                media_library_ids_vec.emplace(v.toNumber());

            auto cluster_ids_vec = std::unordered_set<lms::db::ClusterId>(cluster_ids.size());
            for (auto &v: cluster_ids)
                cluster_ids_vec.emplace(v.toNumber());

            return {lms::db::any_medium::fromString(type, id), weight, value, media_library_ids_vec, cluster_ids_vec};
        }
    };
}

namespace lms::db::tests
{
    class KeywordsFixture : public DatabaseFixture
    {
        std::unordered_map<KeywordIds, Keyword> results;

    protected:
        void collectResults()
        {
            auto transaction{session.createReadTransaction()};

            auto r = session.getDboSession()->query<Keyword>(
                "SELECT type, id, weight, value, media_library_ids, cluster_ids FROM keywords");
            for (auto &kw: r.resultList())
            {
                results.emplace(KeywordIds(kw), kw);
            }
        }

        bool hasResult(AnyMediumId id, const std::string &keyword) const
        {
            return results.contains({id, keyword});
        }

        const Keyword &getResult(AnyMediumId id, const std::string &keyword) const
        {
            return results.at({id, keyword});
        }
    };

    TEST_F(KeywordsFixture, keywords_artist_simple)
    {
        const ScopedArtist artist{session, "MyArtist"};

        collectResults();

        EXPECT_TRUE(hasResult(artist.getId(), "MyArtist"));
        EXPECT_TRUE(getResult(artist.getId(), "MyArtist").cluster_ids.empty());
        EXPECT_TRUE(getResult(artist.getId(), "MyArtist").media_library_ids.empty());
    }

    TEST_F(KeywordsFixture, keywords_artist_with_track_and_media_library)
    {
        ScopedArtist artist{session, "MyArtist"};
        ScopedTrack track{session};
        ScopedMediaLibrary library{session}; {
            auto transaction{session.createWriteTransaction()};
            track.get().modify()->setMediaLibrary(library.get());
            TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
        }

        collectResults();

        EXPECT_EQ(getResult(artist.getId(), "MyArtist").media_library_ids, std::unordered_set{library.getId()});
    }

    TEST_F(KeywordsFixture, keywords_artist_with_track_and_cluster)
    {
        ScopedArtist artist{session, "MyArtist"};
        ScopedTrack track{session};
        ScopedClusterType clusterType{session, "MyClusterType"};
        ScopedCluster cluster{session, clusterType.lockAndGet(), "MyCluster"}; {
            auto transaction{session.createWriteTransaction()};
            cluster.get().modify()->addTrack(track.get());
            TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
        }

        collectResults();

        EXPECT_EQ(getResult(artist.getId(), "MyArtist").cluster_ids, std::unordered_set{cluster.getId()});
    }

    TEST_F(KeywordsFixture, keywords_track)
    {
        ScopedTrack track{session}; {
            auto transaction{session.createWriteTransaction()};
            track.get().modify()->setName("MyTrack");
        }

        collectResults();

        EXPECT_TRUE(hasResult(track.getId(), "MyTrack"));
        EXPECT_TRUE(getResult(track.getId(), "MyTrack").cluster_ids.empty());
        EXPECT_TRUE(getResult(track.getId(), "MyTrack").media_library_ids.empty());
    }

    TEST_F(KeywordsFixture, keywords_track_with_artist)
    {
        ScopedTrack track{session};
        ScopedArtist artist{session, "MyArtist"}; {
            auto transaction{session.createWriteTransaction()};
            TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
        }

        collectResults();

        EXPECT_TRUE(hasResult(track.getId(), "MyArtist"));
        EXPECT_TRUE(getResult(track.getId(), "MyArtist").cluster_ids.empty());
        EXPECT_TRUE(getResult(track.getId(), "MyArtist").media_library_ids.empty());
    }

    TEST_F(KeywordsFixture, keywords_track_with_cluster)
    {
        ScopedTrack track{session};
        ScopedClusterType clusterType{session, "MyClusterType"};
        ScopedCluster cluster{session, clusterType.lockAndGet(), "MyCluster"}; {
            auto transaction{session.createWriteTransaction()};
            track.get().modify()->setName("MyTrack");
            cluster.get().modify()->addTrack(track.get());
        }

        collectResults();

        EXPECT_EQ(getResult(track.getId(), "MyTrack").cluster_ids, std::unordered_set{cluster.getId()});
    }

    TEST_F(KeywordsFixture, keywords_track_with_media_library)
    {
        ScopedTrack track{session};
        ScopedMediaLibrary library{session};

        {
            auto transaction{session.createWriteTransaction()};
            track.get().modify()->setName("MyTrack");
            track.get().modify()->setMediaLibrary(library.get());
        }

        collectResults();

        EXPECT_EQ(getResult(track.getId(), "MyTrack").media_library_ids, std::unordered_set{library.getId()});
    }

    TEST_F(KeywordsFixture, keywords_release)
    {
        ScopedRelease release{ session, "MyRelease" };

        collectResults();

        EXPECT_TRUE(hasResult(release.getId(), "MyRelease"));
        EXPECT_TRUE(getResult(release.getId(), "MyRelease").cluster_ids.empty());
        EXPECT_TRUE(getResult(release.getId(), "MyRelease").media_library_ids.empty());
    }

    TEST_F(KeywordsFixture, keywords_release_with_artist_display_name)
    {
        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{session.createWriteTransaction()};
            release.get().modify()->setArtistDisplayName("MyArtist");
        }

        collectResults();

        EXPECT_TRUE(hasResult(release.getId(), "MyArtist"));
        EXPECT_TRUE(getResult(release.getId(), "MyArtist").cluster_ids.empty());
        EXPECT_TRUE(getResult(release.getId(), "MyArtist").media_library_ids.empty());
    }


    TEST_F(KeywordsFixture, keywords_release_with_track_artist)
    {
        ScopedRelease release{ session, "MyRelease" };
        ScopedTrack track{session};
        ScopedArtist artist{session, "MyArtist"};

        {
            auto transaction{session.createWriteTransaction()};
            track.get().modify()->setRelease(release.get());
            TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
        }

        collectResults();

        EXPECT_TRUE(hasResult(track.getId(), "MyArtist"));
        EXPECT_TRUE(getResult(track.getId(), "MyArtist").cluster_ids.empty());
        EXPECT_TRUE(getResult(track.getId(), "MyArtist").media_library_ids.empty());
    }

    TEST_F(KeywordsFixture, keywords_release_with_cluster)
    {
        ScopedRelease release{ session, "MyRelease" };
        ScopedTrack track{session};
        ScopedClusterType clusterType{session, "MyClusterType"};
        ScopedCluster cluster{session, clusterType.lockAndGet(), "MyCluster"}; {
            auto transaction{session.createWriteTransaction()};
            track.get().modify()->setRelease(release.get());
            cluster.get().modify()->addTrack(track.get());
        }

        collectResults();

        EXPECT_EQ(getResult(release.getId(), "MyRelease").cluster_ids, std::unordered_set{cluster.getId()});
    }

    TEST_F(KeywordsFixture, keywords_release_with_media_library)
    {
        ScopedRelease release{ session, "MyRelease" };
        ScopedTrack track{session};
        ScopedMediaLibrary library{session};

        {
            auto transaction{session.createWriteTransaction()};
            track.get().modify()->setRelease(release.get());
            track.get().modify()->setMediaLibrary(library.get());
        }

        collectResults();

        EXPECT_EQ(getResult(release.getId(), "MyRelease").media_library_ids, std::unordered_set{library.getId()});
    }

    TEST_F(DatabaseFixture, MediumId_find_emptyDatabase)
    {
        auto transaction{session.createReadTransaction()};

        auto result = any_medium::findIds(session, {}, {}, {}, std::nullopt);
        EXPECT_TRUE(result.results.empty());
        EXPECT_FALSE(result.moreResults);
    }


    TEST_F(DatabaseFixture, MediumId_find_no_filters)
    {
        const ScopedArtist artist{session, "MyArtist"};
        const ScopedRelease release{session, "MyRelease"};
        const ScopedTrack track{session};

        const auto expected = std::unordered_set<AnyMediumId>{artist.getId(), release.getId(), track.getId()};

        auto transaction{session.createReadTransaction()};

        const auto result = any_medium::findIds(session, {}, {}, {}, std::nullopt);
        const auto result_set = std::unordered_set<AnyMediumId>{result.results.begin(), result.results.end()};

        EXPECT_EQ(result_set, expected);
        EXPECT_FALSE(result.moreResults);
    }

    TEST_F(DatabaseFixture, MediumId_find_keyword)
    {
        const ScopedArtist artist{session, "MyArtist"};
        const ScopedRelease release{session, "MyRelease"};
        const ScopedTrack track{session};

        const auto expected = std::unordered_set<AnyMediumId>{artist.getId(), release.getId()};

        auto transaction{session.createReadTransaction()};

        const auto result = any_medium::findIds(session, {"My"}, {}, {}, std::nullopt);
        const auto result_set = std::unordered_set<AnyMediumId>{result.results.begin(), result.results.end()};

        EXPECT_EQ(result_set, expected);
        EXPECT_FALSE(result.moreResults);
    }

    TEST_F(DatabaseFixture, MediumId_find_media_library)
    {
        const ScopedArtist artist{session, "MyArtist"};
        const ScopedRelease release{session, "MyRelease"};
        ScopedTrack track{session};
        ScopedMediaLibrary library{session};

        {
            auto transaction{session.createWriteTransaction()};
            track.get().modify()->setMediaLibrary(library.get());
        }

        const auto expected = std::unordered_set<AnyMediumId>{track.getId()};

        auto transaction{session.createReadTransaction()};

        const auto result = any_medium::findIds(session, {}, {}, library.getId(), std::nullopt);
        const auto result_set = std::unordered_set<AnyMediumId>{result.results.begin(), result.results.end()};

        EXPECT_EQ(result_set, expected);
        EXPECT_FALSE(result.moreResults);
    }

    TEST_F(DatabaseFixture, MediumId_find_cluster)
    {
        const ScopedArtist artist{session, "MyArtist"};
        const ScopedRelease release{session, "MyRelease"};
        ScopedTrack track{session};
        ScopedClusterType clusterType{session, "MyClusterType"};
        ScopedCluster cluster{session, clusterType.lockAndGet(), "MyCluster"};

        {
            auto transaction{session.createWriteTransaction()};
            cluster.get().modify()->addTrack(track.get());
        }

        const auto expected = std::unordered_set<AnyMediumId>{track.getId()};

        auto transaction{session.createReadTransaction()};

        ClusterId clusters[1] = {cluster.getId()};
        const auto result = any_medium::findIds(session, {}, clusters, {}, std::nullopt);
        const auto result_set = std::unordered_set<AnyMediumId>{result.results.begin(), result.results.end()};

        EXPECT_EQ(result_set, expected);
        EXPECT_FALSE(result.moreResults);
    }
}
