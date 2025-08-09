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
#include "database/objects/Artist.hpp"

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "core/ILogger.hpp"
#include "database/Session.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/StarredArtist.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/User.hpp"

#include "SqlQuery.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/StringViewTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::Artist)

namespace lms::db
{
    namespace
    {
        template<typename ResultType>
        Wt::Dbo::Query<ResultType> createQuery(Session& session, std::string_view itemToSelect, const Artist::FindParameters& params)
        {
            session.checkReadTransaction();

            auto query{ session.getDboSession()->query<ResultType>("SELECT " + std::string{ itemToSelect } + " FROM artist a") };
            if (params.sortMethod == ArtistSortMethod::LastWrittenDesc
                || params.sortMethod == ArtistSortMethod::AddedDesc
                || params.writtenAfter.isValid()
                || params.linkType
                || params.track.isValid()
                || params.release.isValid()
                || params.filters.clusters.size() == 1
                || params.filters.mediaLibrary.isValid()
                || params.filters.label.isValid()
                || params.filters.releaseType.isValid())
            {
                query.join("track_artist_link t_a_l ON t_a_l.artist_id = a.id");
            }

            if (params.sortMethod == ArtistSortMethod::LastWrittenDesc
                || params.sortMethod == ArtistSortMethod::AddedDesc
                || params.writtenAfter.isValid()
                || params.release.isValid()
                || params.filters.mediaLibrary.isValid()
                || params.filters.label.isValid()
                || params.filters.releaseType.isValid())
            {
                query.join("track t ON t.id = t_a_l.track_id");

                if (params.writtenAfter.isValid())
                    query.where("t.file_last_write > ?").bind(params.writtenAfter);

                if (params.release.isValid())
                    query.where("t.release_id = ?").bind(params.release);

                if (params.filters.mediaLibrary.isValid())
                    query.where("t.media_library_id = ?").bind(params.filters.mediaLibrary);

                if (params.filters.label.isValid())
                {
                    query.join("release_label r_l ON r_l.release_id = t.release_id");
                    query.where("r_l.label_id = ?").bind(params.filters.label);
                }

                if (params.filters.releaseType.isValid())
                {
                    query.join("release_release_type r_r_t ON r_r_t.release_id = t.release_id");
                    query.where("r_r_t.release_type_id = ?").bind(params.filters.releaseType);
                }
            }

            if (params.linkType)
                query.where("+t_a_l.type = ?").bind(*params.linkType); // Exclude this since the query planner does not do a good job when db is not analyzed

            if (!params.keywords.empty())
            {
                std::vector<std::string> clauses;
                std::vector<std::string> sortClauses;

                for (const std::string_view keyword : params.keywords)
                {
                    clauses.push_back("a.name LIKE ? ESCAPE '" ESCAPE_CHAR_STR "'");
                    query.bind("%" + utils::escapeForLikeKeyword(keyword) + "%");
                }

                for (const std::string_view keyword : params.keywords)
                {
                    sortClauses.push_back("a.sort_name LIKE ? ESCAPE '" ESCAPE_CHAR_STR "'");
                    query.bind("%" + utils::escapeForLikeKeyword(keyword) + "%");
                }

                query.where("(" + core::stringUtils::joinStrings(clauses, " AND ") + ") OR (" + core::stringUtils::joinStrings(sortClauses, " AND ") + ")");
            }

            if (params.starringUser.isValid())
            {
                assert(params.feedbackBackend);
                query.join("starred_artist s_a ON s_a.artist_id = a.id")
                    .where("s_a.user_id = ?")
                    .bind(params.starringUser)
                    .where("s_a.backend = ?")
                    .bind(*params.feedbackBackend)
                    .where("s_a.sync_state <> ?")
                    .bind(SyncState::PendingRemove);
            }

            if (params.filters.clusters.size() == 1)
            {
                query.join("track_cluster t_c ON t_c.track_id = t_a_l.track_id")
                    .where("t_c.cluster_id = ?")
                    .bind(params.filters.clusters.front());
            }
            else if (params.filters.clusters.size() > 1)
            {
                std::ostringstream oss;
                oss << "a.id IN (SELECT DISTINCT t_a_l.artist_id FROM track_artist_link t_a_l"
                       " INNER JOIN track_cluster t_c ON t_c.track_id = t_a_l.track_id";

                WhereClause clusterClause;
                for (const ClusterId clusterId : params.filters.clusters)
                {
                    clusterClause.Or(WhereClause("t_c.cluster_id = ?"));
                    query.bind(clusterId);
                }

                oss << " " << clusterClause.get();
                oss << " GROUP BY t_a_l.track_id,t_a_l.artist_id HAVING COUNT(DISTINCT t_c.cluster_id) = " << params.filters.clusters.size() << ")";

                query.where(oss.str());
            }

            if (params.track.isValid())
                query.where("t_a_l.track_id = ?").bind(params.track);

            switch (params.sortMethod)
            {
            case ArtistSortMethod::None:
                break;
            case ArtistSortMethod::Id:
                query.orderBy("a.id");
                break;
            case ArtistSortMethod::Name:
                query.orderBy("a.name COLLATE NOCASE");
                break;
            case ArtistSortMethod::SortName:
                query.orderBy("a.sort_name COLLATE NOCASE");
                break;
            case ArtistSortMethod::Random:
                query.orderBy("RANDOM()");
                break;
            case ArtistSortMethod::LastWrittenDesc:
                query.orderBy("MAX(t.file_last_write) DESC, a.sort_name");
                break;
            case ArtistSortMethod::AddedDesc:
                query.orderBy("MIN(t.file_added) DESC, a.sort_name");
                break;
            case ArtistSortMethod::StarredDateDesc:
                assert(params.starringUser.isValid());
                query.orderBy("s_a.date_time DESC");
                break;
            }

            query.groupBy("a.id");

            return query;
        }

        template<typename ResultType>
        Wt::Dbo::Query<ResultType> createQuery(Session& session, const Artist::FindParameters& params)
        {
            std::string_view itemToSelect;

            if constexpr (std::is_same_v<ResultType, ArtistId>)
                itemToSelect = "a.id";
            else if constexpr (std::is_same_v<ResultType, Wt::Dbo::ptr<Artist>>)
                itemToSelect = "a";
            else
                static_assert("Unhandled type");

            return createQuery<ResultType>(session, itemToSelect, params);
        }
    } // namespace

    Artist::Artist(const std::string& name, const std::optional<core::UUID>& mbid)
        : _mbid{ mbid ? mbid->getAsString() : "" }
    {
        setName(name);
    }

    Artist::pointer Artist::create(Session& session, const std::string& name, const std::optional<core::UUID>& mbid)
    {
        return session.getDboSession()->add(std::unique_ptr<Artist>{ new Artist{ name, mbid } });
    }

    std::size_t Artist::getCount(Session& session)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM artist"));
    }

    void Artist::find(Session& session, ArtistId& lastRetrievedArtist, std::size_t count, const std::function<void(const Artist::pointer&)>& func, MediaLibraryId library)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<Artist>>("SELECT a FROM artist a").orderBy("a.id").where("a.id > ?").bind(lastRetrievedArtist).limit(static_cast<int>(count)) };

        if (library.isValid())
        {
            // Faster than using joins
            query.where("EXISTS (SELECT 1 FROM track_artist_link t_a_l JOIN track t ON t.id = t_a_l.track_id WHERE t_a_l.artist_id = a.id AND t.media_library_id = ?)").bind(library);
        }

        utils::forEachQueryResult(query, [&](const Artist::pointer& artist) {
            func(artist);
            lastRetrievedArtist = artist->getId();
        });
    }

    void Artist::find(Session& session, const IdRange<ArtistId>& idRange, const std::function<void(const Artist::pointer&)>& func)
    {
        assert(idRange.isValid());

        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<Artist>>("SELECT a from artist a").orderBy("a.id").where("a.id BETWEEN ? AND ?").bind(idRange.first).bind(idRange.last) };

        utils::forEachQueryResult(query, [&](const Artist::pointer& artist) {
            func(artist);
        });
    }

    std::vector<Artist::pointer> Artist::find(Session& session, std::string_view name)
    {
        session.checkReadTransaction();

        if (name.size() > maxNameLength)
            name = name.substr(0, maxNameLength);

        return utils::fetchQueryResults<Artist::pointer>(session.getDboSession()->query<Wt::Dbo::ptr<Artist>>("SELECT a FROM artist a").where("a.name = ?").bind(name).orderBy("LENGTH(a.mbid) DESC")); // put mbid entries first
    }

    Artist::pointer Artist::find(Session& session, const core::UUID& mbid)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<Artist>>("SELECT a FROM artist a").where("a.mbid = ?").bind(std::string{ mbid.getAsString() }));
    }

    Artist::pointer Artist::find(Session& session, ArtistId id)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<Artist>>("SELECT a FROM artist a").where("a.id = ?").bind(id));
    }

    RangeResults<ArtistId> Artist::findIds(Session& session, const FindParameters& params)
    {
        session.checkReadTransaction();

        auto query{ createQuery<ArtistId>(session, params) };
        return utils::execRangeQuery<ArtistId>(query, params.range);
    }

    RangeResults<Artist::pointer> Artist::find(Session& session, const FindParameters& params)
    {
        session.checkReadTransaction();

        auto query{ createQuery<Wt::Dbo::ptr<Artist>>(session, params) };
        return utils::execRangeQuery<Artist::pointer>(query, params.range);
    }

    void Artist::find(Session& session, const FindParameters& params, std::function<void(const pointer&)> func)
    {
        session.checkReadTransaction();

        auto query{ createQuery<Wt::Dbo::ptr<Artist>>(session, params) };
        utils::forEachQueryRangeResult(query, params.range, func);
    }

    IdRange<ArtistId> Artist::findNextIdRange(Session& session, ArtistId lastRetrievedId, std::size_t count)
    {
        auto query{ session.getDboSession()->query<std::tuple<ArtistId, ArtistId>>("SELECT MIN(sub.id) AS first_id, MAX(sub.id) AS last_id FROM (SELECT a.id FROM artist a WHERE a.id > ? ORDER BY a.id LIMIT ?) sub") };
        query.bind(lastRetrievedId);
        query.bind(static_cast<int>(count));

        auto res{ utils::fetchQuerySingleResult(query) };
        return IdRange<ArtistId>{ .first = std::get<0>(res), .last = std::get<1>(res) };
    }

    RangeResults<ArtistId> Artist::findOrphanIds(Session& session, std::optional<Range> range)
    {
        session.checkReadTransaction();
        auto query{ session.getDboSession()->query<ArtistId>(R"(SELECT DISTINCT a.id FROM artist a 
WHERE NOT EXISTS (
    SELECT 1 
    FROM track t 
    INNER JOIN track_artist_link t_a_l 
    ON t_a_l.artist_id = a.id 
    WHERE t.id = t_a_l.track_id
)
AND NOT EXISTS (
    SELECT 1 
    FROM artist_info ai 
    WHERE ai.artist_id = a.id))") };
        return utils::execRangeQuery<ArtistId>(query, range);
    }

    bool Artist::exists(Session& session, ArtistId id)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT 1 FROM artist").where("id = ?").bind(id)) == 1;
    }

    void Artist::updatePreferredArtwork(Session& session, ArtistId artistId, ArtworkId artworkId)
    {
        session.checkWriteTransaction();

        if (artworkId.isValid())
            utils::executeCommand(*session.getDboSession(), "UPDATE artist SET preferred_artwork_id = ? WHERE id = ?", artworkId, artistId);
        else
            utils::executeCommand(*session.getDboSession(), "UPDATE artist SET preferred_artwork_id = NULL WHERE id = ?", artistId);
    }

    std::optional<core::UUID> Artist::getMBID() const
    {
        return core::UUID::fromString(_mbid);
    }

    bool Artist::hasMBID() const
    {
        // TODO optim this
        return getMBID().has_value();
    }

    ObjectPtr<Artwork> Artist::getPreferredArtwork() const
    {
        return ObjectPtr<Artwork>{ _preferredArtwork };
    }

    ArtworkId Artist::getPreferredArtworkId() const
    {
        return _preferredArtwork.id();
    }

    RangeResults<ArtistId> Artist::findSimilarArtistIds(core::EnumSet<TrackArtistLinkType> artistLinkTypes, std::optional<Range> range) const
    {
        assert(session());

        std::ostringstream oss;
        oss << "SELECT a.id FROM artist a"
               " INNER JOIN track_artist_link t_a_l ON t_a_l.artist_id = a.id"
               " INNER JOIN track t ON t.id = t_a_l.track_id"
               " INNER JOIN track_cluster t_c ON t_c.track_id = t.id"
               " WHERE "
               " t_c.cluster_id IN (SELECT DISTINCT c.id from cluster c"
               " INNER JOIN track t ON c.id = t_c.cluster_id"
               " INNER JOIN track_cluster t_c ON t_c.track_id = t.id"
               " INNER JOIN artist a ON a.id = t_a_l.artist_id"
               " INNER JOIN track_artist_link t_a_l ON t_a_l.track_id = t.id"
               " WHERE a.id = ?)"
               " AND a.id <> ?";

        if (!artistLinkTypes.empty())
        {
            oss << " AND t_a_l.type IN (";

            bool first{ true };
            for (TrackArtistLinkType type : artistLinkTypes)
            {
                (void)type;
                if (!first)
                    oss << ", ";
                oss << "?";
                first = false;
            }
            oss << ")";
        }

        auto query{ session()->query<ArtistId>(oss.str()).bind(getId()).bind(getId()).groupBy("a.id").orderBy("COUNT(*) DESC, RANDOM()") };

        for (const TrackArtistLinkType type : artistLinkTypes)
            query.bind(type);

        return utils::execRangeQuery<ArtistId>(query, range);
    }

    std::vector<std::vector<Cluster::pointer>> Artist::getClusterGroups(std::span<const ClusterTypeId> clusterTypeIds, std::size_t size) const
    {
        assert(session());

        WhereClause where;

        std::ostringstream oss;
        oss << "SELECT c FROM cluster c INNER JOIN track t ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id INNER JOIN cluster_type c_type ON c.cluster_type_id = c_type.id INNER JOIN artist a ON t_a_l.artist_id = a.id INNER JOIN track_artist_link t_a_l ON t_a_l.track_id = t.id";

        where.And(WhereClause("a.id = ?")).bind(getId().toString());
        {
            WhereClause clusterClause;
            for (const ClusterTypeId clusterTypeId : clusterTypeIds)
                clusterClause.Or(WhereClause("c_type.id = ?")).bind(clusterTypeId.toString());

            where.And(clusterClause);
        }
        oss << " " << where.get();
        oss << "GROUP BY c.id ORDER BY COUNT(DISTINCT c.id) DESC";

        Wt::Dbo::Query<Wt::Dbo::ptr<Cluster>> query{ session()->query<Wt::Dbo::ptr<Cluster>>(oss.str()) };

        for (const std::string& bindArg : where.getBindArgs())
            query.bind(bindArg);

        std::map<ClusterTypeId, std::vector<Cluster::pointer>> clustersByType;
        utils::forEachQueryResult(query, [&](const Cluster::pointer& cluster) {
            if (clustersByType[cluster->getType()->getId()].size() < size)
                clustersByType[cluster->getType()->getId()].push_back(cluster);
        });

        std::vector<std::vector<Cluster::pointer>> res;
        res.reserve(clustersByType.size());
        for (const auto& [clusterTypeId, clusters] : clustersByType)
            res.push_back(clusters);

        return res;
    }

    void Artist::setName(std::string_view name)
    {
        _name.assign(name, 0, maxNameLength);
        LMS_LOG_IF(DB, WARNING, name.size() > maxNameLength, "Artist name too long, truncated to '" << _name << "'");
    }

    void Artist::setSortName(std::string_view sortName)
    {
        _sortName.assign(sortName, 0, maxNameLength);
        LMS_LOG_IF(DB, WARNING, sortName.size() > maxNameLength, "Artist sort name too long, truncated to '" << _sortName << "'");
    }

    void Artist::setPreferredArtwork(ObjectPtr<Artwork> artwork)
    {
        _preferredArtwork = getDboPtr(artwork);
    }
} // namespace lms::db
