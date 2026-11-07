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

#include <sstream>

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "core/ILogger.hpp"
#include "core/String.hpp"

#include "database/Types.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/Genre.hpp"
#include "database/objects/Grouping.hpp"
#include "database/objects/Language.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "database/objects/Medium.hpp"
#include "database/objects/Mood.hpp"
#include "database/objects/Movement.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/TrackEmbeddedImage.hpp"
#include "database/objects/TrackEmbeddedImageLink.hpp"
#include "database/objects/TrackLyrics.hpp"
#include "database/objects/User.hpp"
#include "database/objects/Work.hpp"

#include "JoinSet.hpp"
#include "ObjectPtrImpl.hpp"
#include "Session.hpp"
#include "Utils.hpp"
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

    Track::pointer Track::create(ISession& session)
    {
        return utils::addObject(session, std::make_unique<Track>());
    }

    void Track::destroy(ISession& session, std::span<const TrackId> ids)
    {
        utils::destroyObjects<Track>(session, ids);
    }

    std::size_t Track::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM track"));
    }

    Track::pointer Track::findByPath(ISession& session, const std::filesystem::path& p)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<Wt::Dbo::ptr<Track>>("SELECT t from track t").where("t.absolute_file_path = ?").bind(p));
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

    Track::pointer Track::find(ISession& session, TrackId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<Wt::Dbo::ptr<Track>>("SELECT t from track t").where("t.id = ?").bind(id));
    }

    void Track::find(ISession& session, TrackId& lastRetrievedId, std::size_t count, const std::function<void(const Track::pointer&)>& func, MediaLibraryId library)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<Track>>("SELECT t from track t").orderBy("t.id").where("t.id > ?").bind(lastRetrievedId).limit(static_cast<int>(count)) };

        if (library.isValid())
            query.where("media_library_id = ?").bind(library);

        utils::forEachQueryResult(query, [&](const Track::pointer& track) {
            func(track);
            lastRetrievedId = track->getId();
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

    void Track::find(ISession& session, const IdRange<TrackId>& idRange, const std::function<void(const Track::pointer&)>& func)
    {
        checkReadTransaction(session);

        assert(idRange.isValid());

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<Track>>("SELECT t from track t").orderBy("t.id").where("t.id BETWEEN ? AND ?").bind(idRange.first).bind(idRange.last) };

        utils::forEachQueryResult(query, [&](const Track::pointer& track) {
            func(track);
        });
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

    std::vector<Track::pointer> Track::findByMBID(ISession& session, const core::UUID& mbid)
    {
        checkReadTransaction(session);

        return utils::fetchQueryResults<Track::pointer>(getDboSession(session)->query<Wt::Dbo::ptr<Track>>("SELECT t from track t").where("t.mbid = ?").bind(mbid));
    }

    std::vector<Track::pointer> Track::findByRecordingMBID(ISession& session, const core::UUID& mbid)
    {
        checkReadTransaction(session);

        return utils::fetchQueryResults<Track::pointer>(getDboSession(session)->query<Wt::Dbo::ptr<Track>>("SELECT t from track t").where("t.recording_mbid = ?").bind(mbid));
    }

    std::vector<TrackId> Track::findIdsTrackMBIDDuplicates(ISession& session, std::optional<Range> range)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<TrackId>("SELECT track.id FROM track WHERE mbid in (SELECT mbid FROM track WHERE mbid IS NOT NULL GROUP BY mbid HAVING COUNT (*) > 1)").orderBy("track.release_id,track.mbid") };

        return utils::execRangeQuery<TrackId>(query, range);
    }

    void Track::updatePreferredArtwork(ISession& session, TrackId trackId, ArtworkId artworkId)
    {
        checkWriteTransaction(session);

        if (artworkId.isValid())
            utils::executeCommand(*getDboSession(session), "UPDATE track SET preferred_artwork_id = ? WHERE id = ?", artworkId, trackId);
        else
            utils::executeCommand(*getDboSession(session), "UPDATE track SET preferred_artwork_id = NULL WHERE id = ?", trackId);
    }

    void Track::updatePreferredMediaArtwork(ISession& session, TrackId trackId, ArtworkId artworkId)
    {
        checkWriteTransaction(session);

        if (artworkId.isValid())
            utils::executeCommand(*getDboSession(session), "UPDATE track SET preferred_media_artwork_id = ? WHERE id = ?", artworkId, trackId);
        else
            utils::executeCommand(*getDboSession(session), "UPDATE track SET preferred_media_artwork_id = NULL WHERE id = ?", trackId);
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

    std::vector<Genre::pointer> Track::getGenres() const
    {
        return utils::fetchQueryResults<Genre::pointer>(_genres.find());
    }

    std::vector<GenreId> Track::getGenreIds() const
    {
        assert(session());

        const auto query{ session()->query<GenreId>("SELECT t_g.genre_id FROM track_genre t_g").where("t_g.track_id = ?").bind(getId()) };

        return utils::fetchQueryResults(query);
    }

    std::vector<Grouping::pointer> Track::getGroupings() const
    {
        return utils::fetchQueryResults<Grouping::pointer>(_groupings.find());
    }

    std::vector<GroupingId> Track::getGroupingIds() const
    {
        assert(session());

        const auto query{ session()->query<GroupingId>("SELECT t_gr.grouping_id FROM track_grouping t_gr").where("t_gr.track_id = ?").bind(getId()) };

        return utils::fetchQueryResults(query);
    }

    std::vector<Language::pointer> Track::getLanguages() const
    {
        return utils::fetchQueryResults<Language::pointer>(_languages.find());
    }

    std::vector<LanguageId> Track::getLanguageIds() const
    {
        assert(session());

        const auto query{ session()->query<LanguageId>("SELECT t_l.language_id FROM track_language t_l").where("t_l.track_id = ?").bind(getId()) };

        return utils::fetchQueryResults(query);
    }

    std::vector<Mood::pointer> Track::getMoods() const
    {
        return utils::fetchQueryResults<Mood::pointer>(_moods.find());
    }

    std::vector<MoodId> Track::getMoodIds() const
    {
        assert(session());

        const auto query{ session()->query<MoodId>("SELECT t_m.mood_id FROM track_mood t_m").where("t_m.track_id = ?").bind(getId()) };

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

    std::vector<TrackId> Track::findIds(ISession& session, const FindParameters& parameters)
    {
        checkReadTransaction(session);

        auto query{ createListQuery<TrackId>(session, "t.id", parameters) };
        return utils::execRangeQuery<TrackId>(query, parameters.range);
    }

    std::vector<Track::pointer> Track::find(ISession& session, const FindParameters& parameters)
    {
        checkReadTransaction(session);

        auto query{ createListQuery<Wt::Dbo::ptr<Track>>(session, "t", parameters) };
        return utils::execRangeQuery<Track::pointer>(query, parameters.range);
    }

    void Track::find(ISession& session, const FindParameters& params, const std::function<void(const Track::pointer&)>& func)
    {
        checkReadTransaction(session);

        auto query{ createListQuery<Wt::Dbo::ptr<Track>>(session, "t", params) };
        utils::forEachQueryRangeResult(query, params.range, func);
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

    void Track::setGenres(std::span<const ObjectPtr<Genre>> genres)
    {
        _genres.clear();
        for (const ObjectPtr<Genre>& genre : genres)
            _genres.insert(getDboPtr(genre));
    }

    void Track::setGroupings(std::span<const ObjectPtr<Grouping>> groupings)
    {
        _groupings.clear();
        for (const ObjectPtr<Grouping>& grouping : groupings)
            _groupings.insert(getDboPtr(grouping));
    }

    void Track::setLanguages(std::span<const ObjectPtr<Language>> languages)
    {
        _languages.clear();
        for (const ObjectPtr<Language>& language : languages)
            _languages.insert(getDboPtr(language));
    }

    void Track::setMoods(std::span<const ObjectPtr<Mood>> moods)
    {
        _moods.clear();
        for (const ObjectPtr<Mood>& mood : moods)
            _moods.insert(getDboPtr(mood));
    }

    void Track::setWorks(std::span<const ObjectPtr<Work>> works)
    {
        _works.clear();
        for (const ObjectPtr<Work>& work : works)
            _works.insert(getDboPtr(work));
    }

    std::vector<Work::pointer> Track::getWorks() const
    {
        // deterministic order, callers rely on the first entry
        return utils::fetchQueryResults<Work::pointer>(_works.find().orderBy("id"));
    }

    bool Track::hasWork() const
    {
        return !_works.empty();
    }

    void Track::clearMovements()
    {
        _movements.clear();
    }

    std::vector<Movement::pointer> Track::getMovements() const
    {
        // deterministic order, callers rely on the first entry
        return utils::fetchQueryResults<Movement::pointer>(_movements.find().orderBy("id"));
    }

    bool Track::hasMovement() const
    {
        return !_movements.empty();
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

    void Track::setMediaLibrary(const ObjectPtr<MediaLibrary>& mediaLibrary)
    {
        _mediaLibrary = getDboPtr(mediaLibrary);
    }

    void Track::setDirectory(const ObjectPtr<Directory>& directory)
    {
        _directory = getDboPtr(directory);
    }

    void Track::setPreferredArtwork(const ObjectPtr<Artwork>& artwork)
    {
        _preferredArtwork = getDboPtr(artwork);
    }

    void Track::setPreferredMediaArtwork(const ObjectPtr<Artwork>& artwork)
    {
        _preferredMediaArtwork = getDboPtr(artwork);
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

    bool Track::hasLyrics() const
    {
        return !_trackLyrics.empty();
    }

    std::string_view Track::getCopyright() const
    {
        return _copyright;
    }

    std::string_view Track::getCopyrightURL() const
    {
        return _copyrightURL;
    }

    std::vector<Artist::pointer> Track::getArtists(core::EnumSet<TrackArtistLinkType> linkTypes) const
    {
        assert(session());

        std::ostringstream oss;
        oss << "SELECT a from artist a"
               " INNER JOIN track_artist_link t_a_l ON a.id = t_a_l.artist_id";

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

        query.where("t_a_l.track_id = ?").bind(getId());
        query.groupBy("t_a_l.artist_id");
        query.orderBy("t_a_l.id");

        return utils::fetchQueryResults<Artist::pointer>(query);
    }

    std::vector<ArtistId> Track::getArtistIds(core::EnumSet<TrackArtistLinkType> linkTypes) const
    {
        assert(self());
        assert(session());

        auto query{ session()->query<ArtistId>("SELECT t_a_l.artist_id FROM track_artist_link t_a_l") };

        if (!linkTypes.empty())
        {
            std::ostringstream oss;
            oss << "t_a_l.type IN (";

            bool first{ true };
            for ([[maybe_unused]] TrackArtistLinkType type : linkTypes)
            {
                if (!first)
                    oss << ", ";

                oss << "?";
                first = false;
            }
            oss << ")";

            query.where(oss.str());
            for (TrackArtistLinkType type : linkTypes)
                query.bind(type);
        }

        query.where("t_a_l.track_id = ?").bind(getId());
        query.groupBy("t_a_l.artist_id");
        query.orderBy("t_a_l.id");

        return utils::fetchQueryResults(query);
    }

    std::vector<TrackArtistLink::pointer> Track::getArtistLinks() const
    {
        return utils::fetchQueryResults<TrackArtistLink::pointer>(_trackArtistLinks.find().orderBy("id"));
    }

    void Track::visitArtistLinks(const std::function<void(const ObjectPtr<TrackArtistLink>& artistLink)>& visitor) const
    {
        utils::forEachQueryResult(_trackArtistLinks.find().orderBy("id"), visitor);
    }

    std::vector<ObjectPtr<TrackArtistLink>> Track::getArtistLinks(TrackArtistLinkType type) const
    {
        std::vector<ObjectPtr<TrackArtistLink>> links;
        visitArtistLinks(type, [&links](const ObjectPtr<TrackArtistLink>& artistLink) { links.push_back(artistLink); });
        return links;
    }

    void Track::visitArtistLinks(TrackArtistLinkType type, const std::function<void(const ObjectPtr<TrackArtistLink>& artistLink)>& visitor) const
    {
        auto query{ session()->query<Wt::Dbo::ptr<TrackArtistLink>>("SELECT t_a_l from track_artist_link t_a_l") };
        query.where("t_a_l.track_id = ?").bind(getId());
        query.where("t_a_l.type = ?").bind(type);
        query.orderBy("t_a_l.id");

        return utils::forEachQueryResult(query, visitor);
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
