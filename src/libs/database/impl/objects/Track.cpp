/*
 * Copyright (C) 2013-2016 Emeric Poupon
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

#include "database/objects/Track.hpp"

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "core/ILogger.hpp"
#include "database/Session.hpp"
#include "database/Types.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "database/objects/Medium.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/TrackEmbeddedImage.hpp"
#include "database/objects/TrackEmbeddedImageLink.hpp"
#include "database/objects/TrackFeatures.hpp"
#include "database/objects/TrackLyrics.hpp"
#include "database/objects/User.hpp"

#include "SqlQuery.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/PartialDateTimeTraits.hpp"
#include "traits/PathTraits.hpp"
#include "traits/StringViewTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::Track)

namespace lms::db
{
    namespace
    {
        template<typename ResultType>
        Wt::Dbo::Query<ResultType> createQuery(Session& session, std::string_view itemToSelect, const Track::FindParameters& params)
        {
            session.checkReadTransaction();

            auto query{ session.getDboSession()->query<ResultType>("SELECT " + std::string{ itemToSelect } + " FROM track t") };

            assert(params.keywords.empty() || params.name.empty());
            for (std::string_view keyword : params.keywords)
                query.where("t.name LIKE ? ESCAPE '" ESCAPE_CHAR_STR "'").bind("%" + utils::escapeForLikeKeyword(keyword) + "%");

            if (!params.name.empty())
                query.where("t.name = ?").bind(params.name);

            if (params.writtenAfter.isValid())
                query.where("t.file_last_write > ?").bind(params.writtenAfter);

            if (params.starringUser.isValid())
            {
                assert(params.feedbackBackend);
                query.join("starred_track s_t ON s_t.track_id = t.id")
                    .where("s_t.user_id = ?")
                    .bind(params.starringUser)
                    .where("s_t.backend = ?")
                    .bind(*params.feedbackBackend)
                    .where("s_t.sync_state <> ?")
                    .bind(SyncState::PendingRemove);
            }

            if (params.filters.clusters.size() == 1)
            {
                // optim
                query.join("track_cluster t_c ON t_c.track_id = t.id")
                    .where("t_c.cluster_id = ?")
                    .bind(params.filters.clusters.front());
            }
            else if (params.filters.clusters.size() > 1)
            {
                std::ostringstream oss;
                oss << "t.id IN (SELECT DISTINCT t.id FROM track t"
                       " INNER JOIN track_cluster t_c ON t_c.track_id = t.id";

                WhereClause clusterClause;
                for (const ClusterId clusterId : params.filters.clusters)
                {
                    clusterClause.Or(WhereClause("t_c.cluster_id = ?"));
                    query.bind(clusterId);
                }

                oss << " " << clusterClause.get();
                oss << " GROUP BY t.id HAVING COUNT(*) = " << params.filters.clusters.size() << ")";

                query.where(oss.str());
            }

            if (params.artist.isValid() || !params.artistName.empty())
            {
                query.join("artist a ON a.id = t_a_l.artist_id")
                    .join("track_artist_link t_a_l ON t_a_l.track_id = t.id");

                if (params.artist.isValid())
                    query.where("a.id = ?").bind(params.artist);
                if (!params.artistName.empty())
                    query.where("a.name = ?").bind(params.artistName);

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

                query.groupBy("t.id");
            }

            assert(!(params.nonRelease && params.release.isValid()));
            if (params.nonRelease)
                query.where("t.release_id IS NULL");
            else if (params.release.isValid())
                query.where("t.release_id = ?").bind(params.release);
            else if (!params.releaseName.empty())
            {
                query.join("release r ON t.release_id = r.id");
                query.where("r.name = ?").bind(params.releaseName);
            }

            if (params.medium.isValid())
                query.where("t.medium_id = ?").bind(params.medium);

            if (params.trackList.isValid() || params.sortMethod == TrackSortMethod::TrackList)
            {
                query.join("tracklist t_l ON t_l_e.tracklist_id = t_l.id");
                query.join("tracklist_entry t_l_e ON t.id = t_l_e.track_id");
                query.where("t_l.id = ?").bind(params.trackList);
            }

            if (params.trackNumber)
                query.where("t.track_number = ?").bind(*params.trackNumber);

            if (params.sortMethod == TrackSortMethod::DateDescAndRelease
                || params.sortMethod == TrackSortMethod::Release)
            {
                query.join("medium m ON t.medium_id = m.id");
            }

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

            if (params.directory.isValid())
                query.where("t.directory_id = ?").bind(params.directory);

            if (params.fileSize.has_value())
                query.where("t.file_size = ?").bind(static_cast<long long>(params.fileSize.value()));

            if (params.embeddedImageId.isValid())
            {
                query.join("track_embedded_image_link t_e_i_l ON t_e_i_l.track_id = t.id");
                query.where("t_e_i_l.track_embedded_image_id = ?").bind(params.embeddedImageId);
            }

            switch (params.sortMethod)
            {
            case TrackSortMethod::None:
                break;
            case TrackSortMethod::Id:
                query.orderBy("t.id");
                break;
            case TrackSortMethod::LastWrittenDesc:
                query.orderBy("t.file_last_write DESC");
                break;
            case TrackSortMethod::AddedDesc:
                query.orderBy("t.file_added DESC");
                break;
            case TrackSortMethod::Random:
                query.orderBy("RANDOM()");
                break;
            case TrackSortMethod::StarredDateDesc:
                assert(params.starringUser.isValid());
                query.orderBy("s_t.date_time DESC");
                break;
            case TrackSortMethod::Name:
                query.orderBy("t.name COLLATE NOCASE");
                break;
            case TrackSortMethod::AbsoluteFilePath:
                query.orderBy("t.absolute_file_path COLLATE NOCASE");
                break;
            case TrackSortMethod::DateDescAndRelease:
                query.orderBy("t.date DESC,t.release_id,m.position,t.track_number");
                break;
            case TrackSortMethod::Release:
                query.orderBy("m.position,t.track_number");
                break;
            case TrackSortMethod::TrackList:
                assert(params.trackList.isValid());
                query.orderBy("t_l_e.id");
                break;
            case TrackSortMethod::TrackNumber:
                query.orderBy("t.track_number");
                break;
            }
            return query;
        }

        template<typename ResultType>
        Wt::Dbo::Query<ResultType> createQuery(Session& session, const Track::FindParameters& params)
        {
            std::string_view itemToSelect;

            if constexpr (std::is_same_v<ResultType, TrackId>)
                itemToSelect = "t.id";
            else if constexpr (std::is_same_v<ResultType, Wt::Dbo::ptr<Track>>)
                itemToSelect = "t";
            else
                static_assert("Unhandled type");

            return createQuery<ResultType>(session, itemToSelect, params);
        }
    } // namespace

    Track::pointer Track::create(Session& session)
    {
        return session.getDboSession()->add(std::make_unique<Track>());
    }

    std::size_t Track::getCount(Session& session)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM track"));
    }

    Track::pointer Track::findByPath(Session& session, const std::filesystem::path& p)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<Track>>("SELECT t from track t").where("t.absolute_file_path = ?").bind(p));
    }

    std::optional<FileInfo> Track::findFileInfo(Session& session, const std::filesystem::path& p)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<std::tuple<int, Wt::WDateTime>>("SELECT t.scan_version, t.file_last_write FROM track t WHERE t.absolute_file_path = ?") };
        query.bind(p);

        std::optional<FileInfo> result;

        utils::forEachQueryResult(query, [&](const auto& row) {
            FileInfo info;
            info.scanVersion = std::get<0>(row);
            info.lastWrittenTime = std::get<1>(row);
            result = info;
        });

        return result;
    }

    Track::pointer Track::find(Session& session, TrackId id)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<Track>>("SELECT t from track t").where("t.id = ?").bind(id));
    }

    void Track::find(Session& session, TrackId& lastRetrievedId, std::size_t count, const std::function<void(const Track::pointer&)>& func, MediaLibraryId library)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<Track>>("SELECT t from track t").orderBy("t.id").where("t.id > ?").bind(lastRetrievedId).limit(static_cast<int>(count)) };

        if (library.isValid())
            query.where("media_library_id = ?").bind(library);

        utils::forEachQueryResult(query, [&](const Track::pointer& track) {
            func(track);
            lastRetrievedId = track->getId();
        });
    }

    void Track::findAbsoluteFilePath(Session& session, TrackId& lastRetrievedId, std::size_t count, const std::function<void(TrackId trackId, const std::filesystem::path& absoluteFilePath)>& func)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<std::tuple<TrackId, std::filesystem::path>>("SELECT t.id,t.absolute_file_path from track t").orderBy("t.id").where("t.id > ?").bind(lastRetrievedId).limit(static_cast<int>(count)) };

        utils::forEachQueryResult(query, [&](const auto& res) {
            func(std::get<0>(res), std::get<1>(res));
            lastRetrievedId = std::get<0>(res);
        });
    }

    void Track::find(Session& session, const IdRange<TrackId>& idRange, const std::function<void(const Track::pointer&)>& func)
    {
        assert(idRange.isValid());

        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<Track>>("SELECT t from track t").orderBy("t.id").where("t.id BETWEEN ? AND ?").bind(idRange.first).bind(idRange.last) };

        utils::forEachQueryResult(query, [&](const Track::pointer& track) {
            func(track);
        });
    }

    IdRange<TrackId> Track::findNextIdRange(Session& session, TrackId lastRetrievedId, std::size_t count)
    {
        auto query{ session.getDboSession()->query<std::tuple<TrackId, TrackId>>("SELECT MIN(sub.id) AS first_id, MAX(sub.id) AS last_id FROM (SELECT t.id FROM track t WHERE t.id > ? ORDER BY t.id LIMIT ?) sub") };
        query.bind(lastRetrievedId);
        query.bind(static_cast<int>(count));

        auto res{ utils::fetchQuerySingleResult(query) };
        return IdRange<TrackId>{ .first = std::get<0>(res), .last = std::get<1>(res) };
    }

    bool Track::exists(Session& session, TrackId id)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT 1 from track").where("id = ?").bind(id)) == 1;
    }

    std::vector<Track::pointer> Track::findByMBID(Session& session, const core::UUID& mbid)
    {
        session.checkReadTransaction();

        return utils::fetchQueryResults<Track::pointer>(session.getDboSession()->query<Wt::Dbo::ptr<Track>>("SELECT t from track t").where("t.mbid = ?").bind(mbid.getAsString()));
    }

    std::vector<Track::pointer> Track::findByRecordingMBID(Session& session, const core::UUID& mbid)
    {
        session.checkReadTransaction();

        return utils::fetchQueryResults<Track::pointer>(session.getDboSession()->query<Wt::Dbo::ptr<Track>>("SELECT t from track t").where("t.recording_mbid = ?").bind(mbid.getAsString()));
    }

    RangeResults<TrackId> Track::findIdsTrackMBIDDuplicates(Session& session, std::optional<Range> range)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<TrackId>("SELECT track.id FROM track WHERE mbid in (SELECT mbid FROM track WHERE mbid <> '' GROUP BY mbid HAVING COUNT (*) > 1)").orderBy("track.release_id,track.mbid") };

        return utils::execRangeQuery<TrackId>(query, range);
    }

    RangeResults<TrackId> Track::findIdsWithRecordingMBIDAndMissingFeatures(Session& session, std::optional<Range> range)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<TrackId>("SELECT t.id FROM track t").where("LENGTH(t.recording_mbid) > 0").where("NOT EXISTS (SELECT * FROM track_features t_f WHERE t_f.track_id = t.id)") };

        return utils::execRangeQuery<TrackId>(query, range);
    }

    void Track::updatePreferredArtwork(Session& session, TrackId trackId, ArtworkId artworkId)
    {
        session.checkWriteTransaction();

        if (artworkId.isValid())
            utils::executeCommand(*session.getDboSession(), "UPDATE track SET preferred_artwork_id = ? WHERE id = ?", artworkId, trackId);
        else
            utils::executeCommand(*session.getDboSession(), "UPDATE track SET preferred_artwork_id = NULL WHERE id = ?", trackId);
    }

    void Track::updatePreferredMediaArtwork(Session& session, TrackId trackId, ArtworkId artworkId)
    {
        session.checkWriteTransaction();

        if (artworkId.isValid())
            utils::executeCommand(*session.getDboSession(), "UPDATE track SET preferred_media_artwork_id = ? WHERE id = ?", artworkId, trackId);
        else
            utils::executeCommand(*session.getDboSession(), "UPDATE track SET preferred_media_artwork_id = NULL WHERE id = ?", trackId);
    }

    std::vector<Cluster::pointer> Track::getClusters() const
    {
        return utils::fetchQueryResults<Cluster::pointer>(_clusters.find());
    }

    std::vector<ClusterId> Track::getClusterIds() const
    {
        assert(session());

        const auto query{ session()->query<ClusterId>("SELECT t_c.cluster_id FROM track_cluster t_c").where("t_c.track_id = ?").bind(getId()).groupBy("t_c.cluster_id") };

        return utils::fetchQueryResults(query);
    }

    ObjectPtr<MediaLibrary> Track::getMediaLibrary() const
    {
        return _mediaLibrary;
    }

    ObjectPtr<Directory> Track::getDirectory() const
    {
        return _directory;
    }

    ObjectPtr<Artwork> Track::getPreferredArtwork() const
    {
        return _preferredArtwork;
    }

    ArtworkId Track::getPreferredArtworkId() const
    {
        return _preferredArtwork.id();
    }

    ObjectPtr<Artwork> Track::getPreferredMediaArtwork() const
    {
        return _preferredMediaArtwork;
    }

    ArtworkId Track::getPreferredMediaArtworkId() const
    {
        return _preferredMediaArtwork.id();
    }

    RangeResults<TrackId> Track::findIds(Session& session, const FindParameters& parameters)
    {
        session.checkReadTransaction();

        auto query{ createQuery<TrackId>(session, parameters) };
        return utils::execRangeQuery<TrackId>(query, parameters.range);
    }

    RangeResults<Track::pointer> Track::find(Session& session, const FindParameters& parameters)
    {
        session.checkReadTransaction();

        auto query{ createQuery<Wt::Dbo::ptr<Track>>(session, parameters) };
        return utils::execRangeQuery<Track::pointer>(query, parameters.range);
    }

    void Track::find(Session& session, const FindParameters& params, const std::function<void(const Track::pointer&)>& func)
    {
        session.checkReadTransaction();

        auto query{ createQuery<Wt::Dbo::ptr<Track>>(session, params) };
        utils::forEachQueryRangeResult(query, params.range, func);
    }

    void Track::find(Session& session, const FindParameters& params, bool& moreResults, const std::function<void(const Track::pointer&)>& func)
    {
        session.checkReadTransaction();

        auto query{ createQuery<Wt::Dbo::ptr<Track>>(session, params) };
        utils::forEachQueryRangeResult(query, params.range, moreResults, func);
    }

    RangeResults<TrackId> Track::findSimilarTrackIds(Session& session, const std::vector<TrackId>& tracks, std::optional<Range> range)
    {
        assert(!tracks.empty());
        session.checkReadTransaction();

        std::ostringstream oss;
        for (std::size_t i{}; i < tracks.size(); ++i)
        {
            if (!oss.str().empty())
                oss << ", ";
            oss << "?";
        }

        auto query{ session.getDboSession()->query<TrackId>(
                                               "SELECT t.id FROM track t"
                                               " INNER JOIN track_cluster t_c ON t_c.track_id = t.id"
                                               " AND t_c.cluster_id IN (SELECT DISTINCT c.id FROM cluster c INNER JOIN track_cluster t_c ON t_c.cluster_id = c.id WHERE t_c.track_id IN ("
                                               + oss.str() + "))"
                                                             " AND t.id NOT IN ("
                                               + oss.str() + ")")
                        .groupBy("t.id")
                        .orderBy("COUNT(*) DESC, RANDOM()") };

        for (TrackId trackId : tracks)
            query.bind(trackId);

        for (TrackId trackId : tracks)
            query.bind(trackId);

        return utils::execRangeQuery<TrackId>(query, range);
    }

    void Track::setAbsoluteFilePath(const std::filesystem::path& filePath)
    {
        assert(filePath.is_absolute());
        _absoluteFilePath = filePath;
    }

    void Track::setName(std::string_view name)
    {
        _name = std::string{ name, 0, _maxNameLength };
        LMS_LOG_IF(DB, WARNING, name.size() > _maxNameLength, "Track name too long, truncated to '" << _name << "'");
    }

    void Track::setCopyright(std::string_view copyright)
    {
        _copyright = std::string{ copyright, 0, _maxCopyrightLength };
        LMS_LOG_IF(DB, WARNING, copyright.size() > _maxCopyrightLength, "Track copyright too long, truncated to '" << _copyright << "'");
    }

    void Track::setCopyrightURL(std::string_view copyrightURL)
    {
        _copyrightURL = std::string{ copyrightURL, 0, _maxCopyrightURLLength };
        LMS_LOG_IF(DB, WARNING, copyrightURL.size() > _maxCopyrightURLLength, "Track copyright URL too long, truncated to '" << _copyrightURL << "'");
    }

    void Track::clearArtistLinks()
    {
        _trackArtistLinks.clear();
    }

    void Track::addArtistLink(const ObjectPtr<TrackArtistLink>& artistLink)
    {
        _trackArtistLinks.insert(getDboPtr(artistLink));
    }

    void Track::setClusters(const std::vector<ObjectPtr<Cluster>>& clusters)
    {
        _clusters.clear();
        for (const ObjectPtr<Cluster>& cluster : clusters)
            _clusters.insert(getDboPtr(cluster));
    }

    void Track::clearLyrics()
    {
        _trackLyrics.clear();
    }

    void Track::clearEmbeddedLyrics()
    {
        utils::executeCommand(*session(), "DELETE FROM track_lyrics WHERE absolute_file_path = '' AND track_id = ?", getId());
    }

    void Track::addLyrics(const ObjectPtr<TrackLyrics>& lyrics)
    {
        _trackLyrics.insert(getDboPtr(lyrics));
    }

    void Track::clearEmbeddedImageLinks()
    {
        _embeddedImageLinks.clear();
    }

    void Track::addEmbeddedImageLink(const ObjectPtr<TrackEmbeddedImageLink>& image)
    {
        _embeddedImageLinks.insert(getDboPtr(image));
    }

    void Track::setMediaLibrary(ObjectPtr<MediaLibrary> mediaLibrary)
    {
        _mediaLibrary = getDboPtr(mediaLibrary);
    }

    void Track::setDirectory(ObjectPtr<Directory> directory)
    {
        _directory = getDboPtr(directory);
    }

    void Track::setPreferredArtwork(ObjectPtr<Artwork> artwork)
    {
        _preferredArtwork = getDboPtr(artwork);
    }

    void Track::setPreferredMediaArtwork(ObjectPtr<Artwork> artwork)
    {
        _preferredMediaArtwork = getDboPtr(artwork);
    }

    std::optional<int> Track::getYear() const
    {
        return _date.getYear();
    }

    std::optional<int> Track::getOriginalYear() const
    {
        return _originalDate.getYear();
    }

    bool Track::hasLyrics() const
    {
        return !_trackLyrics.empty();
    }

    std::optional<std::string> Track::getCopyright() const
    {
        return _copyright != "" ? std::make_optional<std::string>(_copyright) : std::nullopt;
    }

    std::optional<std::string> Track::getCopyrightURL() const
    {
        return _copyrightURL != "" ? std::make_optional<std::string>(_copyrightURL) : std::nullopt;
    }

    std::vector<Artist::pointer> Track::getArtists(core::EnumSet<TrackArtistLinkType> linkTypes) const
    {
        assert(session());

        std::ostringstream oss;
        oss << "SELECT a from artist a"
               " INNER JOIN track_artist_link t_a_l ON a.id = t_a_l.artist_id"
               " INNER JOIN track t ON t.id = t_a_l.track_id";

        if (!linkTypes.empty())
        {
            oss << " AND t_a_l.type IN (";

            bool first{ true };
            for ([[maybe_unused]] TrackArtistLinkType type : linkTypes)
            {
                if (!first)
                    oss << ", ";
                oss << "?";
                first = false;
            }
            oss << ")";
        }

        auto query{ session()->query<Wt::Dbo::ptr<Artist>>(oss.str()) };
        for (TrackArtistLinkType type : linkTypes)
            query.bind(type);

        query.where("t.id = ?").bind(getId());
        query.groupBy("t_a_l.artist_id");
        query.orderBy("t_a_l.id");

        return utils::fetchQueryResults<Artist::pointer>(query);
    }

    std::vector<ArtistId> Track::getArtistIds(core::EnumSet<TrackArtistLinkType> linkTypes) const
    {
        assert(self());
        assert(session());

        std::ostringstream oss;
        oss << "SELECT t_a_l.artist_id FROM track_artist_link t_a_l"
               " INNER JOIN track t ON t.id = t_a_l.track_id";

        if (!linkTypes.empty())
        {
            oss << " AND t_a_l.type IN (";

            bool first{ true };
            for ([[maybe_unused]] TrackArtistLinkType type : linkTypes)
            {
                if (!first)
                    oss << ", ";
                oss << "?";
                first = false;
            }
            oss << ")";
        }

        auto query{ session()->query<ArtistId>(oss.str()) };
        for (TrackArtistLinkType type : linkTypes)
            query.bind(type);

        query.where("t.id = ?").bind(getId());
        query.groupBy("t_a_l.artist_id");
        query.orderBy("t_a_l.id");

        return utils::fetchQueryResults(query);
    }

    std::vector<TrackArtistLink::pointer> Track::getArtistLinks() const
    {
        return utils::fetchQueryResults<TrackArtistLink::pointer>(_trackArtistLinks.find());
    }

    std::vector<std::vector<Cluster::pointer>> Track::getClusterGroups(const std::vector<ClusterTypeId>& clusterTypeIds, std::size_t size) const
    {
        assert(self());
        assert(session());

        WhereClause where;

        std::ostringstream oss;

        oss << "SELECT c from cluster c INNER JOIN track t ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id INNER JOIN cluster_type c_type ON c.cluster_type_id = c_type.id";

        where.And(WhereClause("t.id = ?")).bind(getId().toString());
        {
            WhereClause clusterClause;
            for (ClusterTypeId clusterTypeId : clusterTypeIds)
                clusterClause.Or(WhereClause("c_type.id = ?")).bind(clusterTypeId.toString());
            where.And(clusterClause);
        }
        oss << " " << where.get();
        oss << " GROUP BY c.id ORDER BY COUNT(c.id) DESC";

        auto query{ session()->query<Wt::Dbo::ptr<Cluster>>(oss.str()) };
        for (const std::string& bindArg : where.getBindArgs())
            query.bind(bindArg);

        std::map<ClusterTypeId, std::vector<Cluster::pointer>> clusters;
        utils::forEachQueryResult(query, [&](const Cluster::pointer& cluster) {
            if (clusters[cluster->getType()->getId()].size() < size)
                clusters[cluster->getType()->getId()].push_back(cluster);
        });

        std::vector<std::vector<Cluster::pointer>> res;
        for (const auto& [type, clusters] : clusters)
            res.push_back(clusters);

        return res;
    }

    namespace Debug
    {
        std::ostream& operator<<(std::ostream& os, const TrackInfo& trackInfo)
        {
            auto transaction{ trackInfo.session.createReadTransaction() };

            const Track::pointer track{ Track::find(trackInfo.session, trackInfo.trackId) };
            if (track)
            {
                os << track->getName();

                if (const Release::pointer release{ track->getRelease() })
                    os << " [" << release->getName() << "]";
                for (auto artist : track->getArtists({ TrackArtistLinkType::Artist }))
                    os << " - " << artist->getName();
                for (auto cluster : track->getClusters())
                    os << " {" << cluster->getType()->getName() << "-" << cluster->getName() << "}";
            }
            else
            {
                os << "*unknown*";
            }

            return os;
        }
    } // namespace Debug

} // namespace lms::db
