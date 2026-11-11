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
#include "core/String.hpp"

#include "database/Types.hpp"

#include "ArtistDbo.hpp"
#include "ArtworkDbo.hpp"
#include "ClusterDbo.hpp"
#include "DboAccess.hpp"
#include "DirectoryDbo.hpp"
#include "GenreDbo.hpp"
#include "GroupingDbo.hpp"
#include "JoinSet.hpp"
#include "LanguageDbo.hpp"
#include "MediaLibraryDbo.hpp"
#include "MediumDbo.hpp"
#include "MoodDbo.hpp"
#include "MovementDbo.hpp"
#include "ReleaseDbo.hpp"
#include "Session.hpp"
#include "TrackArtistLinkDbo.hpp"
#include "TrackDbo.hpp"
#include "TrackEmbeddedImageDbo.hpp"
#include "TrackEmbeddedImageLinkDbo.hpp"
#include "TrackLyricsDbo.hpp"
#include "Utils.hpp"
#include "WorkDbo.hpp"
#include "objects/FilterExprCompiler.hpp"
#include "objects/detail/Types.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/PartialDateTimeTraits.hpp"
#include "traits/PathTraits.hpp"
#include "traits/StringViewTraits.hpp"
#include "traits/UUIDTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::Track)

namespace lms::db
{
    namespace
    {
        // Several conditions can need the same join, which is then added once
        constexpr std::string_view trackFeedbackJoin{ "track_feedback t_f ON t_f.track_id = t.id" };

        // A track can be linked to the same artist several times (once per link type)
        bool joinsArtists(const Track::FindParameters& params)
        {
            return params.artist.isValid() || !params.artistName.empty();
        }

        // Returns the condition of the parameters, filter included, and adds to joins the joins it relies on
        SqlFragment createParamsConditions(const Track::FindParameters& params, JoinSet& joins)
        {
            assert(params.keywords.empty() || params.name.empty());
            assert(!(params.nonRelease && params.release.isValid()));

            SqlFragment res;

            for (const std::string_view keyword : params.keywords)
                res.combineWithAnd({ "t.name LIKE ? ESCAPE '" ESCAPE_CHAR_STR "'", { "%" + utils::escapeForLikeKeyword(keyword) + "%" } });

            if (!params.name.empty())
                res.combineWithAnd({ "t.name = ?", { params.name } });

            if (params.feedbackUser.isValid())
            {
                joins.add(trackFeedbackJoin);
                res.combineWithAnd({ "t_f.user_id = ?", { params.feedbackUser.getValue() } });
            }
            if (params.feedbackValue)
            {
                joins.add(trackFeedbackJoin);
                res.combineWithAnd({ "t_f.value = ?", { static_cast<int>(*params.feedbackValue) } });
            }

            if (joinsArtists(params))
            {
                joins.add("track_artist_link t_a_l ON t_a_l.track_id = t.id");
                joins.add("artist a ON a.id = t_a_l.artist_id");

                if (params.artist.isValid())
                    res.combineWithAnd({ "a.id = ?", { params.artist.getValue() } });
                if (!params.artistName.empty())
                    res.combineWithAnd({ "a.name = ?", { params.artistName } });

                SqlFragment linkTypes;
                for (const TrackArtistLinkType linkType : params.trackArtistLinkTypes)
                {
                    if (!linkTypes.isEmpty())
                        linkTypes.append(" OR ");
                    linkTypes.append("t_a_l.type = ?", { static_cast<int>(linkType) });
                }
                res.combineWithAnd(linkTypes);
            }

            if (params.nonRelease)
            {
                res.combineWithAnd({ "t.release_id IS NULL" });
            }
            else if (params.release.isValid())
            {
                res.combineWithAnd({ "t.release_id = ?", { params.release.getValue() } });
            }
            else if (!params.releaseName.empty())
            {
                joins.add("release r ON t.release_id = r.id");
                res.combineWithAnd({ "r.name = ?", { params.releaseName } });
            }

            if (params.medium.isValid())
                res.combineWithAnd({ "t.medium_id = ?", { params.medium.getValue() } });

            if (params.trackList.isValid())
            {
                joins.add("tracklist_entry t_l_e ON t_l_e.track_id = t.id");
                res.combineWithAnd({ "t_l_e.tracklist_id = ?", { params.trackList.getValue() } });
            }

            if (params.trackNumber)
                res.combineWithAnd({ "t.track_number = ?", { *params.trackNumber } });

            if (params.directory.isValid())
                res.combineWithAnd({ "t.directory_id = ?", { params.directory.getValue() } });

            if (params.fileSize.has_value())
                res.combineWithAnd({ "t.file_size = ?", { static_cast<long long>(*params.fileSize) } });

            if (params.hasMusicNNEmbeddings.has_value())
            {
                if (*params.hasMusicNNEmbeddings)
                    res.combineWithAnd({ "EXISTS (SELECT t_m_e.track_id FROM track_musicnn_embeddings t_m_e WHERE t_m_e.track_id = t.id)" });
                else
                    res.combineWithAnd({ "NOT EXISTS (SELECT t_m_e.track_id FROM track_musicnn_embeddings t_m_e WHERE t_m_e.track_id = t.id)" });
            }

            if (params.lastTrackId.isValid())
            {
                assert(params.sortMethod == TrackSortMethod::Id);
                res.combineWithAnd({ "t.id > ?", { params.lastTrackId.getValue() } });
            }

            // The joins of a sort select the tracks too: the ones without any row are excluded
            switch (params.sortMethod)
            {
            case TrackSortMethod::FeedbackDateDesc:
                joins.add(trackFeedbackJoin);
                break;
            case TrackSortMethod::DateDescAndRelease:
            case TrackSortMethod::OriginalDateDescAndRelease:
            case TrackSortMethod::Release:
                joins.add("medium m ON t.medium_id = m.id");
                break;
            case TrackSortMethod::None:
            case TrackSortMethod::Id:
            case TrackSortMethod::Random:
            case TrackSortMethod::LastWrittenDesc:
            case TrackSortMethod::AddedDesc:
            case TrackSortMethod::AbsoluteFilePath:
            case TrackSortMethod::Name:
            case TrackSortMethod::TrackNumber:
                break;
            }

            if (params.filter)
                res.combineWithAnd(compileFilterExpr(*params.filter, params.filterUser));

            return res;
        }

        template<typename ResultType>
        void applySortMethod(Wt::Dbo::Query<ResultType>& query, const Track::FindParameters& params)
        {
            switch (params.sortMethod)
            {
            case TrackSortMethod::None:
                if (params.trackList.isValid())
                    query.orderBy("t_l_e.id");
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
            case TrackSortMethod::FeedbackDateDesc:
                assert(params.feedbackUser.isValid());
                query.orderBy("t_f.date_time DESC");
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
            case TrackSortMethod::OriginalDateDescAndRelease:
                query.orderBy("COALESCE(t.original_date, t.date) DESC,t.release_id,m.position,t.track_number");
                break;
            case TrackSortMethod::Release:
                query.orderBy("m.position,t.track_number");
                break;
            case TrackSortMethod::TrackNumber:
                query.orderBy("t.track_number");
                break;
            }
        }

        template<typename ResultType>
        Wt::Dbo::Query<ResultType> createListQuery(ISession& session, std::string_view selectedItem, const Track::FindParameters& params)
        {
            JoinSet joins;
            const SqlFragment where{ createParamsConditions(params, joins) };

            auto query{ getDboSession(session)->query<ResultType>("SELECT " + std::string{ selectedItem } + " FROM track t") };
            joins.apply(query);
            applyWhereFragment(query, where);
            applySortMethod(query, params);

            if (joinsArtists(params))
                query.groupBy("t.id");

            return query;
        }

        Wt::Dbo::Query<int> createCountQuery(ISession& session, const Track::FindParameters& params)
        {
            JoinSet joins;
            const SqlFragment where{ createParamsConditions(params, joins) };

            auto query{ getDboSession(session)->query<int>(joinsArtists(params) ? "SELECT COUNT(DISTINCT t.id) FROM track t" : "SELECT COUNT(*) FROM track t") };
            joins.apply(query);
            applyWhereFragment(query, where);

            return query;
        }
    } // namespace

    Track Track::create(ISession& session)
    {
        return utils::addObject(session, std::make_unique<dbo::Track>());
    }

    void Track::destroy(ISession& session, std::span<const TrackId> ids)
    {
        utils::destroyObjects<dbo::Track>(session, ids);
    }

    std::size_t Track::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM track"));
    }

    std::optional<Track> Track::findByPath(ISession& session, const std::filesystem::path& p)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleObject(getDboSession(session)->query<Wt::Dbo::ptr<dbo::Track>>("SELECT t from track t").where("t.absolute_file_path = ?").bind(p));
    }

    std::optional<FileInfo> Track::findFileInfo(ISession& session, const std::filesystem::path& p)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<std::tuple<int, Wt::WDateTime>>("SELECT t.scan_version, t.file_last_write FROM track t WHERE t.absolute_file_path = ?") };
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

    std::optional<Track> Track::find(ISession& session, TrackId id)
    {
        checkReadTransaction(session);

        if (!id.isValid())
            return std::nullopt;

        return utils::fetchQuerySingleObject(getDboSession(session)->query<Wt::Dbo::ptr<dbo::Track>>("SELECT t from track t").where("t.id = ?").bind(id));
    }

    void Track::find(ISession& session, TrackId& lastRetrievedId, std::size_t count, const std::function<void(const Track&)>& func, MediaLibraryId library)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::Track>>("SELECT t from track t").orderBy("t.id").where("t.id > ?").bind(lastRetrievedId).limit(static_cast<int>(count)) };

        if (library.isValid())
            query.where("media_library_id = ?").bind(library);

        utils::forEachQueryObject(query, [&](const Track& track) {
            func(track);
            lastRetrievedId = track.getId();
        });
    }

    void Track::findAbsoluteFilePath(ISession& session, TrackId& lastRetrievedId, std::size_t count, const TrackLocationVisitor& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<std::tuple<TrackId, std::filesystem::path>>("SELECT t.id,t.absolute_file_path from track t").orderBy("t.id").where("t.id > ?").bind(lastRetrievedId).limit(static_cast<int>(count)) };

        utils::forEachQueryResult(query, [&](const auto& res) {
            func(std::get<0>(res), std::get<1>(res));
            lastRetrievedId = std::get<0>(res);
        });
    }

    void Track::findAbsoluteFilePath(ISession& session, const FindParameters& params, const TrackLocationVisitor& func)
    {
        checkReadTransaction(session);

        auto query{ createListQuery<std::tuple<TrackId, std::filesystem::path>>(session, "t.id, t.absolute_file_path", params) };

        utils::forEachQueryRangeResult(query, params.range, [&](const auto& res) {
            func(std::get<0>(res), std::get<1>(res));
        });
    }

    void Track::find(ISession& session, const IdRange<TrackId>& idRange, const std::function<void(const Track&)>& func)
    {
        checkReadTransaction(session);

        assert(idRange.isValid());

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::Track>>("SELECT t from track t").orderBy("t.id").where("t.id BETWEEN ? AND ?").bind(idRange.first).bind(idRange.last) };

        utils::forEachQueryObject(query, func);
    }

    IdRange<TrackId> Track::findNextIdRange(ISession& session, TrackId lastRetrievedId, std::size_t count)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<std::tuple<TrackId, TrackId>>("SELECT MIN(sub.id) AS first_id, MAX(sub.id) AS last_id FROM (SELECT t.id FROM track t WHERE t.id > ? ORDER BY t.id LIMIT ?) sub") };
        query.bind(lastRetrievedId);
        query.bind(static_cast<int>(count));

        auto res{ utils::fetchQuerySingleResult(query) };
        return IdRange<TrackId>{ .first = std::get<0>(res), .last = std::get<1>(res) };
    }

    bool Track::exists(ISession& session, TrackId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT 1 from track").where("id = ?").bind(id)) == 1;
    }

    std::vector<Track> Track::findByMBID(ISession& session, const core::UUID& mbid)
    {
        checkReadTransaction(session);

        return utils::fetchQueryObjects(getDboSession(session)->query<Wt::Dbo::ptr<dbo::Track>>("SELECT t from track t").where("t.mbid = ?").bind(mbid));
    }

    std::vector<Track> Track::findByRecordingMBID(ISession& session, const core::UUID& mbid)
    {
        checkReadTransaction(session);

        return utils::fetchQueryObjects(getDboSession(session)->query<Wt::Dbo::ptr<dbo::Track>>("SELECT t from track t").where("t.recording_mbid = ?").bind(mbid));
    }

    std::vector<TrackId> Track::findIdsTrackMBIDDuplicates(ISession& session, std::optional<Range> range)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<TrackId>("SELECT track.id FROM track WHERE mbid in (SELECT mbid FROM track WHERE mbid IS NOT NULL GROUP BY mbid HAVING COUNT (*) > 1)").orderBy("track.release_id,track.mbid") };

        return utils::execRangeQuery<TrackId>(query, range);
    }

    void Track::updatePreferredArtwork(ISession& session, TrackId trackId, ArtworkId artworkId)
    {
        dbo::Track& track{ utils::modifyObject<dbo::Track>(session, trackId) };
        setRelation(*getDboSession(session), track._preferredArtwork, artworkId);
    }

    void Track::updatePreferredMediaArtwork(ISession& session, TrackId trackId, ArtworkId artworkId)
    {
        dbo::Track& track{ utils::modifyObject<dbo::Track>(session, trackId) };
        setRelation(*getDboSession(session), track._preferredMediaArtwork, artworkId);
    }

    std::vector<TrackId> Track::findIds(ISession& session, const FindParameters& parameters)
    {
        checkReadTransaction(session);

        auto query{ createListQuery<TrackId>(session, "t.id", parameters) };
        return utils::execRangeQuery<TrackId>(query, parameters.range);
    }

    std::vector<Track> Track::find(ISession& session, const FindParameters& parameters)
    {
        checkReadTransaction(session);

        auto query{ createListQuery<Wt::Dbo::ptr<dbo::Track>>(session, "t", parameters) };
        return utils::execRangeObjectQuery(query, parameters.range);
    }

    void Track::find(ISession& session, const FindParameters& params, const std::function<void(const Track&)>& func)
    {
        checkReadTransaction(session);

        auto query{ createListQuery<Wt::Dbo::ptr<dbo::Track>>(session, "t", params) };
        utils::forEachQueryRangeObject(query, params.range, func);
    }

    std::size_t Track::getCount(ISession& session, const FindParameters& params)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(createCountQuery(session, params));
    }

    void Track::setAbsoluteFilePath(const std::filesystem::path& filePath)
    {
        assert(filePath.is_absolute());
        _absoluteFilePath = filePath;
    }

    void Track::setContainer(core::media::Container container)
    {
        _container = detail::getDbContainer(container);
    }

    void Track::setCodec(core::media::Codec codec)
    {
        _codec = detail::getDbCodec(codec);
    }

    void Track::setName(std::string_view name)
    {
        _name = core::stringUtils::utf8Truncate(name, _maxNameLength);
        LMS_LOG_IF(DB, WARNING, name.size() > _maxNameLength, "Track name too long, truncated to '" << _name << "'");
    }

    void Track::setCopyright(std::string_view copyright)
    {
        _copyright = core::stringUtils::utf8Truncate(copyright, _maxCopyrightLength);
        LMS_LOG_IF(DB, WARNING, copyright.size() > _maxCopyrightLength, "Track copyright too long, truncated to '" << _copyright << "'");
    }

    void Track::setCopyrightURL(std::string_view copyrightURL)
    {
        _copyrightURL = core::stringUtils::utf8Truncate(copyrightURL, _maxCopyrightURLLength);
        LMS_LOG_IF(DB, WARNING, copyrightURL.size() > _maxCopyrightURLLength, "Track copyright URL too long, truncated to '" << _copyrightURL << "'");
    }

    void dbo::Track::applyIds()
    {
        Wt::Dbo::Session& dboSession{ *session() };
        setRelation(dboSession, _release, _releaseId);
        setRelation(dboSession, _medium, _mediumId);
        setRelation(dboSession, _mediaLibrary, _mediaLibraryId);
        setRelation(dboSession, _directory, _directoryId);
        setRelation(dboSession, _preferredArtwork, _preferredArtworkId);
        setRelation(dboSession, _preferredMediaArtwork, _preferredMediaArtworkId);
    }

    void Track::update(ISession& session, const Track& track)
    {
        utils::updateObject<dbo::Track>(session, track);
    }

    namespace
    {
        template<typename DboType, typename IdType>
        void setCollection(Wt::Dbo::Session& session, Wt::Dbo::collection<Wt::Dbo::ptr<DboType>>& collection, std::span<const IdType> ids)
        {
            collection.clear();
            for (const IdType id : ids)
                collection.insert(session.loadLazy<DboType>(id.getValue()));
        }
    } // namespace

    void Track::clearArtistLinks(ISession& session, TrackId id)
    {
        utils::modifyObject<dbo::Track>(session, id)._trackArtistLinks.clear();
    }

    void Track::setClusters(ISession& session, TrackId id, std::span<const ClusterId> clusters)
    {
        setCollection(*getDboSession(session), utils::modifyObject<dbo::Track>(session, id)._clusters, clusters);
    }

    void Track::setGenres(ISession& session, TrackId id, std::span<const GenreId> genres)
    {
        setCollection(*getDboSession(session), utils::modifyObject<dbo::Track>(session, id)._genres, genres);
    }

    void Track::setGroupings(ISession& session, TrackId id, std::span<const GroupingId> groupings)
    {
        setCollection(*getDboSession(session), utils::modifyObject<dbo::Track>(session, id)._groupings, groupings);
    }

    void Track::setLanguages(ISession& session, TrackId id, std::span<const LanguageId> languages)
    {
        setCollection(*getDboSession(session), utils::modifyObject<dbo::Track>(session, id)._languages, languages);
    }

    void Track::setMoods(ISession& session, TrackId id, std::span<const MoodId> moods)
    {
        setCollection(*getDboSession(session), utils::modifyObject<dbo::Track>(session, id)._moods, moods);
    }

    void Track::setWorks(ISession& session, TrackId id, std::span<const WorkId> works)
    {
        setCollection(*getDboSession(session), utils::modifyObject<dbo::Track>(session, id)._works, works);
    }

    void Track::clearMovements(ISession& session, TrackId id)
    {
        utils::modifyObject<dbo::Track>(session, id)._movements.clear();
    }

    void Track::clearEmbeddedLyrics(ISession& session, TrackId id)
    {
        checkWriteTransaction(session);
        utils::executeCommand(*getDboSession(session), "DELETE FROM track_lyrics WHERE absolute_file_path = '' AND track_id = ?", id);
    }

    void Track::clearEmbeddedImageLinks(ISession& session, TrackId id)
    {
        utils::modifyObject<dbo::Track>(session, id)._embeddedImageLinks.clear();
    }

    bool Track::hasLyrics(ISession& session, TrackId id)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT EXISTS (SELECT 1 FROM track_lyrics WHERE track_id = ?)").bind(id)) == 1;
    }

    bool Track::hasWork(ISession& session, TrackId id)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT EXISTS (SELECT 1 FROM track_work WHERE track_id = ?)").bind(id)) == 1;
    }

    bool Track::hasMovement(ISession& session, TrackId id)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT EXISTS (SELECT 1 FROM track_movement WHERE track_id = ?)").bind(id)) == 1;
    }

    std::optional<core::media::Container> Track::getContainer() const
    {
        return detail::getMediaContainerType(_container);
    }

    std::optional<core::media::Codec> Track::getCodec() const
    {
        return detail::getMediaCodecType(_codec);
    }

    std::optional<int> Track::getYear() const
    {
        return _date.getYear();
    }

    std::optional<int> Track::getOriginalYear() const
    {
        return _originalDate.getYear();
    }
} // namespace lms::db
