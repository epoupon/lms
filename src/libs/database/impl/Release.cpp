/*
 * Copyright (C) 2015 Emeric Poupon
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

#include "database/Release.hpp"

#include <Wt/Dbo/WtSqlTraits.h>

#include "core/ILogger.hpp"
#include "database/Artist.hpp"
#include "database/Cluster.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/User.hpp"

#include "EnumSetTraits.hpp"
#include "IdTypeTraits.hpp"
#include "SqlQuery.hpp"
#include "StringViewTraits.hpp"
#include "Utils.hpp"

namespace lms::db
{
    namespace
    {
        template<typename ResultType>
        Wt::Dbo::Query<ResultType> createQuery(Session& session, std::string_view itemToSelect, const Release::FindParameters& params)
        {
            auto query{ session.getDboSession()->query<ResultType>("SELECT " + std::string{ itemToSelect } + " from release r") };

            if (params.sortMethod == ReleaseSortMethod::ArtistNameThenName
                || params.sortMethod == ReleaseSortMethod::LastWritten
                || params.sortMethod == ReleaseSortMethod::DateAsc
                || params.sortMethod == ReleaseSortMethod::DateDesc
                || params.sortMethod == ReleaseSortMethod::OriginalDate
                || params.sortMethod == ReleaseSortMethod::OriginalDateDesc
                || params.writtenAfter.isValid()
                || params.dateRange
                || params.artist.isValid()
                || params.clusters.size() == 1
                || params.mediaLibrary.isValid()
                || params.directory.isValid())
            {
                query.join("track t ON t.release_id = r.id");
            }

            if (params.mediaLibrary.isValid())
                query.where("t.media_library_id = ?").bind(params.mediaLibrary);

            if (params.directory.isValid())
                query.where("t.directory_id = ?").bind(params.directory);

            if (!params.releaseType.empty())
            {
                query.join("release_release_type r_r_t ON r_r_t.release_id = r.id");
                query.join("release_type r_t ON r_t.id = r_r_t.release_type_id")
                    .where("r_t.name = ?")
                    .bind(params.releaseType);
            }

            if (params.writtenAfter.isValid())
                query.where("t.file_last_write > ?").bind(params.writtenAfter);

            if (params.dateRange)
            {
                query.where("COALESCE(CAST(SUBSTR(t.date, 1, 4) AS INTEGER), t.year) >= ?").bind(params.dateRange->begin);
                query.where("COALESCE(CAST(SUBSTR(t.date, 1, 4) AS INTEGER), t.year) <= ?").bind(params.dateRange->end);
            }

            for (std::string_view keyword : params.keywords)
                query.where("r.name LIKE ? ESCAPE '" ESCAPE_CHAR_STR "'").bind("%" + utils::escapeLikeKeyword(keyword) + "%");

            if (params.starringUser.isValid())
            {
                assert(params.feedbackBackend);
                query.join("starred_release s_r ON s_r.release_id = r.id")
                    .where("s_r.user_id = ?")
                    .bind(params.starringUser)
                    .where("s_r.backend = ?")
                    .bind(*params.feedbackBackend)
                    .where("s_r.sync_state <> ?")
                    .bind(SyncState::PendingRemove);
            }

            if (params.artist.isValid()
                || params.sortMethod == ReleaseSortMethod::ArtistNameThenName)
            {
                query.join("track_artist_link t_a_l ON t_a_l.track_id = t.id");

                if (params.artist.isValid())
                    query.where("t_a_l.artist_id = ?").bind(params.artist);

                if (params.sortMethod == ReleaseSortMethod::ArtistNameThenName)
                    query.join("artist a ON a.id = t_a_l.artist_id");

                if (!params.trackArtistLinkTypes.empty())
                {
                    std::ostringstream oss;

                    bool first{ true };
                    for (TrackArtistLinkType linkType : params.trackArtistLinkTypes)
                    {
                        if (!first)
                            oss << " OR ";
                        oss << "t_a_l.type = ?";
                        query.bind(linkType);

                        first = false;
                    }
                    query.where(oss.str());
                }

                if (!params.excludedTrackArtistLinkTypes.empty())
                {
                    std::ostringstream oss;
                    oss << "r.id NOT IN (SELECT DISTINCT r.id FROM release r"
                           " INNER JOIN track_artist_link t_a_l ON t_a_l.track_id = t.id"
                           " INNER JOIN track t ON t.release_id = r.id"
                           " WHERE (t_a_l.artist_id = ? AND (";

                    query.bind(params.artist);

                    bool first{ true };
                    for (const TrackArtistLinkType linkType : params.excludedTrackArtistLinkTypes)
                    {
                        if (!first)
                            oss << " OR ";
                        oss << "t_a_l.type = ?";
                        query.bind(linkType);

                        first = false;
                    }
                    oss << ")))";
                    query.where(oss.str());
                }
            }

            if (params.clusters.size() == 1)
            {
                query.join("track_cluster t_c ON t_c.track_id = t.id")
                    .where("t_c.cluster_id = ?")
                    .bind(params.clusters.front());
            }
            else if (params.clusters.size() > 1)
            {
                std::ostringstream oss;
                oss << "r.id IN (SELECT DISTINCT t.release_id FROM track t"
                       " INNER JOIN track_cluster t_c ON t_c.track_id = t.id";

                WhereClause clusterClause;
                for (const ClusterId clusterId : params.clusters)
                {
                    clusterClause.Or(WhereClause("t_c.cluster_id = ?"));
                    query.bind(clusterId);
                }

                oss << " " << clusterClause.get();
                oss << " GROUP BY t.id HAVING COUNT(*) = " << params.clusters.size() << ")";

                query.where(oss.str());
            }

            switch (params.sortMethod)
            {
            case ReleaseSortMethod::None:
                break;
            case ReleaseSortMethod::Id:
                query.orderBy("r.id");
                break;
            case ReleaseSortMethod::Name:
                query.orderBy("r.name COLLATE NOCASE");
                break;
            case ReleaseSortMethod::ArtistNameThenName:
                query.orderBy("a.name COLLATE NOCASE, r.name COLLATE NOCASE");
                break;
            case ReleaseSortMethod::Random:
                query.orderBy("RANDOM()");
                break;
            case ReleaseSortMethod::LastWritten:
                query.orderBy("t.file_last_write DESC");
                break;
            case ReleaseSortMethod::DateAsc:
                query.orderBy("COALESCE(t.date, CAST(t.year AS TEXT)) ASC, r.name COLLATE NOCASE");
                break;
            case ReleaseSortMethod::DateDesc:
                query.orderBy("COALESCE(t.date, CAST(t.year AS TEXT)) DESC, r.name COLLATE NOCASE");
                break;
            case ReleaseSortMethod::OriginalDate:
                query.orderBy("COALESCE(original_date, CAST(original_year AS TEXT), date, CAST(year AS TEXT)), r.name COLLATE NOCASE");
                break;
            case ReleaseSortMethod::OriginalDateDesc:
                query.orderBy("COALESCE(original_date, CAST(original_year AS TEXT), date, CAST(year AS TEXT)) DESC, r.name COLLATE NOCASE");
                break;
            case ReleaseSortMethod::StarredDateDesc:
                assert(params.starringUser.isValid());
                query.orderBy("s_r.date_time DESC");
                break;
            }

            return query;
        }
    } // namespace

    ReleaseType::ReleaseType(std::string_view name)
        : _name{ name }
    {
        // As we use the name to uniquely identoify release type, we must throw (and not truncate)
        if (name.size() > _maxNameLength)
            throw Exception{ "ReleaseType name is too long: " + std::string{ name } + "'" };
    }

    ReleaseType::pointer ReleaseType::create(Session& session, std::string_view name)
    {
        return session.getDboSession()->add(std::unique_ptr<ReleaseType>{ new ReleaseType{ name } });
    }

    ReleaseType::pointer ReleaseType::find(Session& session, ReleaseTypeId id)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<ReleaseType>>("SELECT r_t from release_type r_t").where("r_t.id = ?").bind(id));
    }

    ReleaseType::pointer ReleaseType::find(Session& session, std::string_view name)
    {
        session.checkReadTransaction();

        if (name.size() > _maxNameLength)
            throw Exception{ "Requeted ReleaseType name is too long: " + std::string{ name } + "'" };

        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<ReleaseType>>("SELECT r_t from release_type r_t").where("r_t.name = ?").bind(name));
    }

    Release::Release(const std::string& name, const std::optional<core::UUID>& MBID)
        : _name{ std::string(name, 0, _maxNameLength) }
        , _MBID{ MBID ? MBID->getAsString() : "" }
    {
    }

    Release::pointer Release::create(Session& session, const std::string& name, const std::optional<core::UUID>& MBID)
    {
        return session.getDboSession()->add(std::unique_ptr<Release>{ new Release{ name, MBID } });
    }

    std::vector<Release::pointer> Release::find(Session& session, const std::string& name, const std::filesystem::path& releaseDirectory)
    {
        session.checkReadTransaction();

        return utils::fetchQueryResults<Release::pointer>(session.getDboSession()->query<Wt::Dbo::ptr<Release>>("SELECT DISTINCT r from release r").join("track t ON t.release_id = r.id").where("r.name = ?").bind(std::string(name, 0, _maxNameLength)).where("t.absolute_file_path LIKE ? ESCAPE '" ESCAPE_CHAR_STR "'").bind(utils::escapeLikeKeyword(releaseDirectory.string()) + "%"));
    }

    Release::pointer Release::find(Session& session, const core::UUID& mbid)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<Release>>("SELECT r from release r").where("r.mbid = ?").bind(mbid.getAsString()));
    }

    Release::pointer Release::find(Session& session, ReleaseId id)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<Release>>("SELECT r from release r").where("r.id = ?").bind(id));
    }

    bool Release::exists(Session& session, ReleaseId id)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT 1 FROM release").where("id = ?").bind(id)) == 1;
    }

    std::size_t Release::getCount(Session& session)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM release"));
    }

    RangeResults<ReleaseId> Release::findOrphanIds(Session& session, std::optional<Range> range)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<ReleaseId>("select r.id from release r LEFT OUTER JOIN Track t ON r.id = t.release_id WHERE t.id IS NULL") };
        return utils::execRangeQuery<ReleaseId>(query, range);
    }

    void Release::find(Session& session, ReleaseId& lastRetrievedRelease, std::size_t count, const std::function<void(const Release::pointer&)>& func, MediaLibraryId library)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<Release>>("SELECT r FROM release r").orderBy("r.id").where("r.id > ?").bind(lastRetrievedRelease).limit(static_cast<int>(count)) };

        if (library.isValid())
        {
            // Faster than using joins
            query.where("EXISTS (SELECT 1 FROM track t WHERE t.release_id = r.id AND t.media_library_id = ?)").bind(library);
        }

        utils::forEachQueryResult(query, [&](const Release::pointer& release) {
            func(release);
            lastRetrievedRelease = release->getId();
        });
    }

    RangeResults<Release::pointer> Release::find(Session& session, const FindParameters& params)
    {
        session.checkReadTransaction();

        auto query{ createQuery<Wt::Dbo::ptr<Release>>(session, "DISTINCT r", params) };
        return utils::execRangeQuery<pointer>(query, params.range);
    }

    void Release::find(Session& session, const FindParameters& params, const std::function<void(const pointer&)>& func)
    {
        session.checkReadTransaction();

        auto query{ createQuery<Wt::Dbo::ptr<Release>>(session, "DISTINCT r", params) };
        utils::forEachQueryRangeResult(query, params.range, func);
    }

    RangeResults<ReleaseId> Release::findIds(Session& session, const FindParameters& params)
    {
        session.checkReadTransaction();

        auto query{ createQuery<ReleaseId>(session, "DISTINCT r.id", params) };
        return utils::execRangeQuery<ReleaseId>(query, params.range);
    }

    std::size_t Release::getCount(Session& session, const FindParameters& params)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(createQuery<int>(session, "COUNT(DISTINCT r.id)", params));
    }

    std::size_t Release::getDiscCount() const
    {
        assert(session());
        int res{ utils::fetchQuerySingleResult(session()->query<int>("SELECT COUNT(DISTINCT disc_number) FROM track t").where("t.release_id = ?").bind(getId())) };
        return res;
    }

    std::vector<DiscInfo> Release::getDiscs() const
    {
        assert(session());

        using ResultType = std::tuple<int, std::string>;
        const auto query{ session()->query<ResultType>("SELECT DISTINCT disc_number, disc_subtitle FROM track t").where("t.release_id = ?").bind(getId()).orderBy("disc_number") };

        std::vector<DiscInfo> discs;
        utils::forEachQueryResult(query, [&](ResultType&& res) {
            discs.emplace_back(DiscInfo{ static_cast<std::size_t>(std::get<int>(res)), std::move(std::get<std::string>(res)) });
        });

        return discs;
    }

    Wt::WDate Release::getDate() const
    {
        return getDate(false);
    }

    Wt::WDate Release::getOriginalDate() const
    {
        return getDate(true);
    }

    Wt::WDate Release::getDate(bool original) const
    {
        assert(session());

        const char* field{ original ? "original_date" : "date" };
        auto query{ (session()->query<Wt::WDate>(std::string{ "SELECT " } + "t." + field + " FROM track t").where("t.release_id = ?").groupBy(field).bind(getId())) };

        const auto dates{ utils::fetchQueryResults(query) };

        // various dates => invalid date
        if (dates.empty() || dates.size() > 1)
            return {};

        return dates.front();
    }

    std::optional<int> Release::getYear() const
    {
        return getYear(false);
    }

    std::optional<int> Release::getOriginalYear() const
    {
        return getYear(true);
    }

    std::optional<int> Release::getYear(bool original) const
    {
        assert(session());

        const char* field{ original ? "original_year" : "year" };
        auto query{ session()->query<std::optional<int>>(std::string{ "SELECT " } + "t." + field + " FROM track t").where("t.release_id = ?").bind(getId()).groupBy(field) };

        const auto years{ utils::fetchQueryResults(query) };

        // various years => invalid years
        const std::size_t count{ years.size() };
        if (count == 0 || count > 1)
            return std::nullopt;

        return years.front();
    }

    std::optional<std::string> Release::getCopyright() const
    {
        assert(session());

        auto query{ session()->query<std::string>("SELECT copyright FROM track t INNER JOIN release r ON r.id = t.release_id").where("r.id = ?").groupBy("copyright").bind(getId()) };

        const auto copyrights{ utils::fetchQueryResults(query) };

        // various copyrights => no copyright
        if (copyrights.empty() || copyrights.size() > 1 || copyrights.front().empty())
            return std::nullopt;

        return std::move(copyrights.front());
    }

    std::optional<std::string> Release::getCopyrightURL() const
    {
        assert(session());

        const auto query{ session()->query<std::string>("SELECT copyright_url FROM track t INNER JOIN release r ON r.id = t.release_id").where("r.id = ?").bind(getId()).groupBy("copyright_url") };

        const auto copyrights{ utils::fetchQueryResults(query) };

        // various copyright URLs => no copyright URL
        if (copyrights.empty() || copyrights.size() > 1 || copyrights.front().empty())
            return std::nullopt;

        return std::move(copyrights.front());
    }

    std::size_t Release::getMeanBitrate() const
    {
        assert(session());

        return utils::fetchQuerySingleResult(session()->query<int>("SELECT COALESCE(AVG(t.bitrate), 0) FROM track t").where("release_id = ?").bind(getId()).where("bitrate > 0"));
    }

    std::vector<Artist::pointer> Release::getArtists(TrackArtistLinkType linkType) const
    {
        assert(session());

        const auto query{ session()->query<Wt::Dbo::ptr<Artist>>(
                                       "SELECT a FROM artist a"
                                       " INNER JOIN track_artist_link t_a_l ON t_a_l.artist_id = a.id"
                                       " INNER JOIN track t ON t.id = t_a_l.track_id")
                              .where("t.release_id = ?")
                              .bind(getId())
                              .where("+t_a_l.type = ?")
                              .bind(linkType) // adding + since the query planner does not a good job when analyze is not performed
                              .groupBy("a.id") };

        return utils::fetchQueryResults<Artist::pointer>(query);
    }

    std::vector<Release::pointer> Release::getSimilarReleases(std::optional<std::size_t> offset, std::optional<std::size_t> count) const
    {
        assert(session());

        // Select the similar releases using the 5 most used clusters of the release
        auto query{ session()->query<Wt::Dbo::ptr<Release>>(
                                 "SELECT r FROM release r"
                                 " INNER JOIN track t ON t.release_id = r.id"
                                 " INNER JOIN track_cluster t_c ON t_c.track_id = t.id"
                                 " WHERE "
                                 " t_c.cluster_id IN "
                                 "(SELECT DISTINCT c.id FROM cluster c"
                                 " INNER JOIN track t ON c.id = t_c.cluster_id"
                                 " INNER JOIN track_cluster t_c ON t_c.track_id = t.id"
                                 " INNER JOIN release r ON r.id = t.release_id"
                                 " WHERE r.id = ?)"
                                 " AND r.id <> ?")
                        .bind(getId())
                        .bind(getId())
                        .groupBy("r.id")
                        .orderBy("COUNT(*) DESC, RANDOM()")
                        .limit(count ? static_cast<int>(*count) : -1)
                        .offset(offset ? static_cast<int>(*offset) : -1) };

        return utils::fetchQueryResults<Release::pointer>(query);
    }

    void Release::clearReleaseTypes()
    {
        _releaseTypes.clear();
    }

    void Release::addReleaseType(ObjectPtr<ReleaseType> releaseType)
    {
        _releaseTypes.insert(getDboPtr(releaseType));
    }

    bool Release::hasVariousArtists() const
    {
        // TODO optimize
        return getArtists().size() > 1;
    }

    bool Release::hasDiscSubtitle() const
    {
        return utils::fetchQuerySingleResult(session()->query<int>("SELECT EXISTS (SELECT 1 FROM track WHERE disc_subtitle IS NOT NULL AND disc_subtitle <> '' AND release_id = ?)").bind(getId()));
    }

    std::size_t Release::getTrackCount() const
    {
        assert(session());
        return utils::fetchQuerySingleResult(session()->query<int>("SELECT COUNT(t.id) FROM track t INNER JOIN release r ON r.id = t.release_id").where("r.id = ?").bind(getId()));
    }

    std::vector<ReleaseType::pointer> Release::getReleaseTypes() const
    {
        // TODO remove?
        return utils::fetchQueryResults<ReleaseType::pointer>(_releaseTypes.find());
    }

    std::vector<std::string> Release::getReleaseTypeNames() const
    {
        std::vector<std::string> res;

        for (const auto& releaseType : _releaseTypes)
            res.push_back(std::string{ releaseType->getName() });

        return res;
    }

    std::chrono::milliseconds Release::getDuration() const
    {
        assert(session());

        using milli = std::chrono::duration<int, std::milli>;

        return utils::fetchQuerySingleResult(session()->query<milli>("SELECT COALESCE(SUM(duration), 0) FROM track t").where("t.release_id = ?").bind(getId()));
    }

    Wt::WDateTime Release::getLastWritten() const
    {
        assert(session());

        return utils::fetchQuerySingleResult(session()->query<Wt::WDateTime>("SELECT COALESCE(MAX(file_last_write), '1970-01-01T00:00:00') FROM track t").where("t.release_id = ?").bind(getId()));
    }

    std::vector<std::vector<Cluster::pointer>> Release::getClusterGroups(const std::vector<ClusterTypeId>& clusterTypeIds, std::size_t size) const
    {
        assert(session());

        WhereClause where;

        std::ostringstream oss;

        oss << "SELECT c from cluster c INNER JOIN track_cluster t_c ON t_c.cluster_id = c.id INNER JOIN track t ON t.id = t_c.track_id ";

        where.And(WhereClause("t.release_id = ?")).bind(getId().toString());
        {
            WhereClause clusterClause;
            for (const ClusterTypeId clusterTypeId : clusterTypeIds)
                clusterClause.Or(WhereClause("c.cluster_type_id = ?")).bind(clusterTypeId.toString());
            where.And(clusterClause);
        }
        oss << " " << where.get();
        oss << " GROUP BY c.id ORDER BY COUNT(c.id) DESC";

        auto query{ session()->query<Wt::Dbo::ptr<Cluster>>(oss.str()) };
        for (const std::string& bindArg : where.getBindArgs())
            query.bind(bindArg);

        auto queryRes{ query.resultList() };

        std::map<ClusterTypeId, std::vector<Cluster::pointer>> clustersByType;
        for (const Wt::Dbo::ptr<Cluster>& cluster : queryRes)
        {
            if (clustersByType[cluster->getType()->getId()].size() < size)
                clustersByType[cluster->getType()->getId()].push_back(cluster);
        }

        std::vector<std::vector<Cluster::pointer>> res;
        for (const auto& [clusterTypeId, clusters] : clustersByType)
            res.push_back(clusters);

        return res;
    }

} // namespace lms::db
