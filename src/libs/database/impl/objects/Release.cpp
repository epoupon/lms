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

#include "database/objects/Release.hpp"

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "core/PartialDateTime.hpp"
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
#include "ReleaseArtistLinkDbo.hpp"
#include "ReleaseDbo.hpp"
#include "Session.hpp"
#include "TrackArtistLinkDbo.hpp"
#include "TrackDbo.hpp"
#include "TrackEmbeddedImageDbo.hpp"
#include "TrackEmbeddedImageLinkDbo.hpp"
#include "TrackLyricsDbo.hpp"
#include "Utils.hpp"
#include "WorkDbo.hpp"
#include "detail/Types.hpp"
#include "objects/FilterExprCompiler.hpp"
#include "traits/EnumSetTraits.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/PartialDateTimeTraits.hpp"
#include "traits/StringViewTraits.hpp"
#include "traits/UUIDTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::Country)
LMS_DB_INSTANTIATE_OBJECT(lms::db::Label)
LMS_DB_INSTANTIATE_OBJECT(lms::db::Release)
LMS_DB_INSTANTIATE_OBJECT(lms::db::ReleaseType)

namespace lms::db
{
    namespace
    {
        // Several conditions can need the same join, which is then added once
        constexpr std::string_view tracksJoin{ "track t ON t.release_id = r.id" };
        constexpr std::string_view releaseFeedbackJoin{ "release_feedback r_f ON r_f.release_id = r.id" };

        SqlFragment createKeywordsCondition(const std::vector<std::string_view>& keywords)
        {
            const auto createKeywordClauses{ [&](std::string_view column) {
                SqlFragment res;
                for (const std::string_view keyword : keywords)
                {
                    if (!res.isEmpty())
                        res.append(" AND ");
                    res.append(std::string{ column } + " LIKE ? ESCAPE '" ESCAPE_CHAR_STR "'", { "%" + utils::escapeForLikeKeyword(keyword) + "%" });
                }
                return res;
            } };

            SqlFragment res{ "(" };
            res.append(createKeywordClauses("r.name"));
            res.append(") OR (");
            res.append(createKeywordClauses("m.name"));
            res.append(")");
            return res;
        }

        // Returns the condition of the parameters, filter included, and adds to joins the joins it relies on
        SqlFragment createParamsConditions(const Release::FindParameters& params, JoinSet& joins)
        {
            assert(params.keywords.empty() || params.name.empty());
            assert(!params.directory.isValid() || !params.parentDirectory.isValid());
            assert(!params.artist.isValid() || !params.trackArtist.isValid());

            SqlFragment res;

            if (params.directory.isValid())
            {
                joins.add(tracksJoin);
                res.combineWithAnd({ "t.directory_id = ?", { params.directory.getValue() } });
            }

            if (params.parentDirectory.isValid())
            {
                joins.add(tracksJoin);
                joins.add("directory d ON t.directory_id = d.id");
                res.combineWithAnd({ "d.parent_directory_id = ?", { params.parentDirectory.getValue() } });
            }

            if (params.trackArtist.isValid())
            {
                joins.add(tracksJoin);
                joins.add("track_artist_link t_a_l ON t_a_l.track_id = t.id");
                res.combineWithAnd({ "t_a_l.artist_id = ?", { params.trackArtist.getValue() } });

                SqlFragment linkTypes;
                for (const TrackArtistLinkType linkType : params.trackArtistLinkTypes)
                {
                    if (!linkTypes.isEmpty())
                        linkTypes.append(" OR ");
                    linkTypes.append("t_a_l.type = ?", { static_cast<int>(linkType) });
                }
                res.combineWithAnd(linkTypes);
            }

            if (!params.releaseType.empty())
            {
                joins.add("release_release_type r_r_t ON r_r_t.release_id = r.id");
                joins.add("release_type r_t ON r_t.id = r_r_t.release_type_id");
                res.combineWithAnd({ "r_t.name = ?", { params.releaseType } });
            }

            if (!params.name.empty())
                res.combineWithAnd({ "r.name = ?", { params.name } });

            if (!params.keywords.empty())
            {
                joins.add("medium m ON m.release_id = r.id", JoinSet::Type::Left);
                res.combineWithAnd(createKeywordsCondition(params.keywords));
            }

            if (params.feedbackUser.isValid())
            {
                joins.add(releaseFeedbackJoin);
                res.combineWithAnd({ "r_f.user_id = ?", { params.feedbackUser.getValue() } });
            }
            if (params.feedbackValue)
            {
                joins.add(releaseFeedbackJoin);
                res.combineWithAnd({ "r_f.value = ?", { static_cast<int>(*params.feedbackValue) } });
            }

            if (params.artist.isValid())
            {
                joins.add("release_artist_link r_a_l ON r_a_l.release_id = r.id");
                res.combineWithAnd({ "r_a_l.artist_id = ?", { params.artist.getValue() } });
            }

            if (params.releaseGroupMBID)
                res.combineWithAnd({ "r.group_mbid = ?", { *params.releaseGroupMBID } });

            // The joins of a sort select the releases too: the ones without any row are excluded
            switch (params.sortMethod)
            {
            case ReleaseSortMethod::LastWrittenDesc:
            case ReleaseSortMethod::AddedDesc:
            case ReleaseSortMethod::DateAsc:
            case ReleaseSortMethod::DateDesc:
            case ReleaseSortMethod::OriginalDate:
            case ReleaseSortMethod::OriginalDateDesc:
                joins.add(tracksJoin);
                break;
            case ReleaseSortMethod::FeedbackDateDesc:
                joins.add(releaseFeedbackJoin);
                break;
            case ReleaseSortMethod::None:
            case ReleaseSortMethod::Id:
            case ReleaseSortMethod::Name:
            case ReleaseSortMethod::SortName:
            case ReleaseSortMethod::ArtistNameThenName:
            case ReleaseSortMethod::Random:
                break;
            }

            if (params.filter)
                res.combineWithAnd(createMatchingTracksCondition("r.id", *params.filter, params.filterUser, "t.release_id"));

            return res;
        }

        template<typename ResultType>
        void applySortMethod(Wt::Dbo::Query<ResultType>& query, const Release::FindParameters& params)
        {
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
            case ReleaseSortMethod::SortName:
                query.orderBy("COALESCE(NULLIF(r.sort_name, ''), r.name) COLLATE NOCASE");
                break;
            case ReleaseSortMethod::ArtistNameThenName:
                query.orderBy("COALESCE(NULLIF(r.artist_display_name, ''), r.name) COLLATE NOCASE, COALESCE(NULLIF(r.sort_name, ''), r.name) COLLATE NOCASE");
                break;
            case ReleaseSortMethod::Random:
                query.orderBy("RANDOM()");
                break;
            case ReleaseSortMethod::LastWrittenDesc:
                query.orderBy("MAX(t.file_last_write) DESC, COALESCE(NULLIF(r.sort_name, ''), r.name) COLLATE NOCASE");
                break;
            case ReleaseSortMethod::AddedDesc:
                query.orderBy("MIN(t.file_added) DESC, COALESCE(NULLIF(r.sort_name, ''), r.name) COLLATE NOCASE");
                break;
            case ReleaseSortMethod::DateAsc:
                query.orderBy("MIN(t.date) ASC, COALESCE(NULLIF(r.sort_name, ''), r.name) COLLATE NOCASE");
                break;
            case ReleaseSortMethod::DateDesc:
                query.orderBy("MIN(t.date) DESC, COALESCE(NULLIF(r.sort_name, ''), r.name) COLLATE NOCASE");
                break;
            case ReleaseSortMethod::OriginalDate:
                query.orderBy("MIN(COALESCE(t.original_date, t.date)) ASC, COALESCE(NULLIF(r.sort_name, ''), r.name) COLLATE NOCASE");
                break;
            case ReleaseSortMethod::OriginalDateDesc:
                query.orderBy("MIN(COALESCE(t.original_date, t.date)) DESC, COALESCE(NULLIF(r.sort_name, ''), r.name) COLLATE NOCASE");
                break;
            case ReleaseSortMethod::FeedbackDateDesc:
                assert(params.feedbackUser.isValid());
                query.orderBy("r_f.date_time DESC");
                break;
            }
        }

        template<typename ResultType>
        Wt::Dbo::Query<ResultType> createListQuery(ISession& session, std::string_view selectedItem, const Release::FindParameters& params)
        {
            JoinSet joins;
            const SqlFragment where{ createParamsConditions(params, joins) };

            auto query{ getDboSession(session)->query<ResultType>("SELECT " + std::string{ selectedItem } + " from release r") };
            joins.apply(query);
            applyWhereFragment(query, where);
            applySortMethod(query, params);

            // Joins can duplicate releases
            if (!joins.empty())
                query.groupBy("r.id");

            return query;
        }

        Wt::Dbo::Query<int> createCountQuery(ISession& session, const Release::FindParameters& params)
        {
            JoinSet joins;
            const SqlFragment where{ createParamsConditions(params, joins) };

            // Joins can duplicate releases
            auto query{ getDboSession(session)->query<int>(joins.empty() ? "SELECT COUNT(*) from release r" : "SELECT COUNT(DISTINCT r.id) from release r") };
            joins.apply(query);
            applyWhereFragment(query, where);

            return query;
        }

    } // namespace

    Country::Country(std::string_view name)
        : _name{ name }
    {
        // As we use the name to uniquely identoify release type, we must throw (and not truncate)
        if (name.size() > _maxNameLength)
            throw Exception{ "Country name is too long: '" + std::string{ name } + "'" };
    }

    Country Country::create(ISession& session, std::string_view name)
    {
        return utils::addObject(session, std::unique_ptr<dbo::Country>{ new dbo::Country{ name } });
    }

    void Country::destroy(ISession& session, std::span<const CountryId> ids)
    {
        utils::destroyObjects<dbo::Country>(session, ids);
    }

    std::size_t Country::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM country"));
    }

    std::optional<Country> Country::find(ISession& session, CountryId id)
    {
        checkReadTransaction(session);

        if (!id.isValid())
            return std::nullopt;

        return utils::fetchQuerySingleObject(getDboSession(session)->query<Wt::Dbo::ptr<dbo::Country>>("SELECT c from country c").where("c.id = ?").bind(id));
    }

    std::optional<Country> Country::find(ISession& session, std::string_view name)
    {
        checkReadTransaction(session);

        if (name.size() > _maxNameLength)
            throw Exception{ "Country name is too long: '" + std::string{ name } + "'" };

        return utils::fetchQuerySingleObject(getDboSession(session)->query<Wt::Dbo::ptr<dbo::Country>>("SELECT c from country c").where("c.name = ?").bind(name));
    }

    void Country::find(ISession& session, ReleaseId release, const std::function<void(const Country&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::Country>>("SELECT c FROM country c").join("release_country r_c ON r_c.country_id = c.id").where("r_c.release_id = ?").bind(release) };
        utils::forEachQueryObject(query, func);
    }

    std::vector<CountryId> Country::findOrphanIds(ISession& session, std::optional<Range> range)
    {
        checkReadTransaction(session);

        // select the labels that have no releases
        auto query{ getDboSession(session)->query<CountryId>("select c.id from country c LEFT OUTER JOIN release_country r_c ON c.id = r_c.country_id WHERE r_c.release_id IS NULL") };
        return utils::execRangeQuery<CountryId>(query, range);
    }

    Label::Label(std::string_view name)
        : _name{ name }
    {
        // As we use the name to uniquely identify this label, we must throw (and not truncate)
        if (name.size() > _maxNameLength)
            throw Exception{ "Label name is too long: '" + std::string{ name } + "'" };
    }

    Label Label::create(ISession& session, std::string_view name)
    {
        return utils::addObject(session, std::unique_ptr<dbo::Label>{ new dbo::Label{ name } });
    }

    void Label::destroy(ISession& session, std::span<const LabelId> ids)
    {
        utils::destroyObjects<dbo::Label>(session, ids);
    }

    std::size_t Label::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM label"));
    }

    std::optional<Label> Label::find(ISession& session, LabelId id)
    {
        checkReadTransaction(session);

        if (!id.isValid())
            return std::nullopt;

        return utils::fetchQuerySingleObject(getDboSession(session)->query<Wt::Dbo::ptr<dbo::Label>>("SELECT l from label l").where("l.id = ?").bind(id));
    }

    std::optional<Label> Label::find(ISession& session, std::string_view name)
    {
        checkReadTransaction(session);

        if (name.size() > _maxNameLength)
            throw Exception{ "Label name is too long: '" + std::string{ name } + "'" };

        return utils::fetchQuerySingleObject(getDboSession(session)->query<Wt::Dbo::ptr<dbo::Label>>("SELECT l from label l").where("l.name = ?").bind(name));
    }

    void Label::find(ISession& session, LabelSortMethod sortMethod, const std::function<void(const Label& label)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->find<dbo::Label>() };
        switch (sortMethod)
        {
        case LabelSortMethod::None:
            break;
        case LabelSortMethod::Name:
            query.orderBy("name COLLATE NOCASE");
        }

        utils::forEachQueryObject(query, func);
    }

    void Label::find(ISession& session, ReleaseId release, const std::function<void(const Label&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::Label>>("SELECT l FROM label l").join("release_label r_l ON r_l.label_id = l.id").where("r_l.release_id = ?").bind(release) };
        utils::forEachQueryObject(query, func);
    }

    std::vector<LabelId> Label::findOrphanIds(ISession& session, std::optional<Range> range)
    {
        checkReadTransaction(session);

        // select the labels that have no releases
        auto query{ getDboSession(session)->query<LabelId>("select l.id from label l LEFT OUTER JOIN release_label r_l ON l.id = r_l.label_id WHERE r_l.release_id IS NULL") };
        return utils::execRangeQuery<LabelId>(query, range);
    }

    ReleaseType::ReleaseType(std::string_view name)
        : _name{ name }
    {
        // As we use the name to uniquely identify release types, we must throw (and not truncate)
        if (name.size() > _maxNameLength)
            throw Exception{ "ReleaseType is too long: '" + std::string{ name } + "'" };
    }

    ReleaseType ReleaseType::create(ISession& session, std::string_view name)
    {
        return utils::addObject(session, std::unique_ptr<dbo::ReleaseType>{ new dbo::ReleaseType{ name } });
    }

    void ReleaseType::destroy(ISession& session, std::span<const ReleaseTypeId> ids)
    {
        utils::destroyObjects<dbo::ReleaseType>(session, ids);
    }

    std::size_t ReleaseType::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM release_type"));
    }

    std::optional<ReleaseType> ReleaseType::find(ISession& session, ReleaseTypeId id)
    {
        checkReadTransaction(session);

        if (!id.isValid())
            return std::nullopt;

        return utils::fetchQuerySingleObject(getDboSession(session)->query<Wt::Dbo::ptr<dbo::ReleaseType>>("SELECT r_t from release_type r_t").where("r_t.id = ?").bind(id));
    }

    std::optional<ReleaseType> ReleaseType::find(ISession& session, std::string_view name)
    {
        checkReadTransaction(session);

        if (name.size() > _maxNameLength)
            throw Exception{ "ReleaseType is too long: '" + std::string{ name } + "'" };

        return utils::fetchQuerySingleObject(getDboSession(session)->query<Wt::Dbo::ptr<dbo::ReleaseType>>("SELECT r_t from release_type r_t").where("r_t.name = ?").bind(name));
    }

    void ReleaseType::find(ISession& session, ReleaseTypeSortMethod sortMethod, const std::function<void(const ReleaseType& releaseType)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->find<dbo::ReleaseType>() };
        switch (sortMethod)
        {
        case ReleaseTypeSortMethod::None:
            break;
        case ReleaseTypeSortMethod::Name:
            query.orderBy("name COLLATE NOCASE");
        }

        utils::forEachQueryObject(query, func);
    }

    void ReleaseType::find(ISession& session, ReleaseId release, const std::function<void(const ReleaseType&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::ReleaseType>>("SELECT r_t FROM release_type r_t").join("release_release_type r_r_t ON r_r_t.release_type_id = r_t.id").where("r_r_t.release_id = ?").bind(release) };
        utils::forEachQueryObject(query, func);
    }

    std::vector<ReleaseTypeId> ReleaseType::findOrphanIds(ISession& session, std::optional<Range> range)
    {
        checkReadTransaction(session);

        // select the release types that have no releases
        auto query{ getDboSession(session)->query<ReleaseTypeId>("select r_t.id from release_type r_t LEFT OUTER JOIN release_release_type r_r_t ON r_t.id = r_r_t.release_type_id WHERE r_r_t.release_id IS NULL") };
        return utils::execRangeQuery<ReleaseTypeId>(query, range);
    }

    Release::Release(const std::string& name, const std::optional<core::UUID>& MBID)
        : _name{ core::stringUtils::utf8Truncate(name, _maxNameLength) }
        , _MBID{ MBID }
    {
    }

    Release Release::create(ISession& session, const std::string& name, const std::optional<core::UUID>& MBID)
    {
        return utils::addObject(session, std::unique_ptr<dbo::Release>{ new dbo::Release{ name, MBID } });
    }

    void Release::destroy(ISession& session, std::span<const ReleaseId> ids)
    {
        utils::destroyObjects<dbo::Release>(session, ids);
    }

    std::optional<Release> Release::find(ISession& session, const core::UUID& mbid)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleObject(getDboSession(session)->query<Wt::Dbo::ptr<dbo::Release>>("SELECT r from release r").where("r.mbid = ?").bind(mbid));
    }

    std::optional<Release> Release::find(ISession& session, ReleaseId id)
    {
        checkReadTransaction(session);

        if (!id.isValid())
            return std::nullopt;

        return utils::fetchQuerySingleObject(getDboSession(session)->query<Wt::Dbo::ptr<dbo::Release>>("SELECT r from release r").where("r.id = ?").bind(id));
    }

    bool Release::exists(ISession& session, ReleaseId id)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT 1 FROM release").where("id = ?").bind(id)) == 1;
    }

    std::size_t Release::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM release"));
    }

    std::vector<ReleaseId> Release::findOrphanIds(ISession& session, std::optional<Range> range)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<ReleaseId>("select r.id from release r LEFT OUTER JOIN Track t ON r.id = t.release_id WHERE t.id IS NULL") };
        return utils::execRangeQuery<ReleaseId>(query, range);
    }

    void Release::find(ISession& session, ReleaseId& lastRetrievedRelease, std::size_t count, const std::function<void(const Release&)>& func, MediaLibraryId library)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::Release>>("SELECT r FROM release r").orderBy("r.id").where("r.id > ?").bind(lastRetrievedRelease).limit(static_cast<int>(count)) };

        if (library.isValid())
        {
            // Faster than using joins
            query.where("EXISTS (SELECT 1 FROM track t WHERE t.release_id = r.id AND t.media_library_id = ?)").bind(library);
        }

        utils::forEachQueryObject(query, [&](const Release& release) {
            func(release);
            lastRetrievedRelease = release.getId();
        });
    }

    void Release::find(ISession& session, const IdRange<ReleaseId>& idRange, const std::function<void(const Release&)>& func)
    {
        checkReadTransaction(session);

        assert(idRange.isValid());

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::Release>>("SELECT r from release r").orderBy("r.id").where("r.id BETWEEN ? AND ?").bind(idRange.first).bind(idRange.last) };

        utils::forEachQueryObject(query, func);
    }

    IdRange<ReleaseId> Release::findNextIdRange(ISession& session, ReleaseId lastRetrievedId, std::size_t count)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<std::tuple<ReleaseId, ReleaseId>>("SELECT MIN(sub.id) AS first_id, MAX(sub.id) AS last_id FROM (SELECT r.id FROM release r WHERE r.id > ? ORDER BY r.id LIMIT ?) sub") };
        query.bind(lastRetrievedId);
        query.bind(static_cast<int>(count));

        auto res{ utils::fetchQuerySingleResult(query) };
        return IdRange<ReleaseId>{ .first = std::get<0>(res), .last = std::get<1>(res) };
    }

    std::vector<Release> Release::find(ISession& session, const FindParameters& params)
    {
        checkReadTransaction(session);

        auto query{ createListQuery<Wt::Dbo::ptr<dbo::Release>>(session, "r", params) };
        return utils::execRangeObjectQuery(query, params.range);
    }

    void Release::find(ISession& session, const FindParameters& params, const std::function<void(const Release&)>& func)
    {
        checkReadTransaction(session);

        auto query{ createListQuery<Wt::Dbo::ptr<dbo::Release>>(session, "r", params) };
        utils::forEachQueryRangeObject(query, params.range, func);
    }

    std::vector<ReleaseId> Release::findIds(ISession& session, const FindParameters& params)
    {
        checkReadTransaction(session);

        auto query{ createListQuery<ReleaseId>(session, "r.id", params) };
        return utils::execRangeQuery<ReleaseId>(query, params.range);
    }

    std::size_t Release::getCount(ISession& session, const FindParameters& params)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(createCountQuery(session, params));
    }

    bool Release::hasArtist(ISession& session, ReleaseId id, ArtistId artistId)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<int>("SELECT COUNT(1) FROM release_artist_link r_a_l") };
        query.where("r_a_l.release_id = ?").bind(id);
        query.where("r_a_l.artist_id = ?").bind(artistId);

        return utils::fetchQuerySingleResult(query) > 0;
    }

    namespace
    {
        core::PartialDateTime getDate(ISession& session, ReleaseId id, bool original)
        {
            checkReadTransaction(session);

            const char* field{ original ? "original_date" : "date" };
            auto query{ (getDboSession(session)->query<core::PartialDateTime>(std::string{ "SELECT " } + "t." + field + " FROM track t").where("t.release_id = ?").groupBy(field).bind(id)) };

            const auto dates{ utils::fetchQueryResults(query) };

            // various dates => invalid date
            if (dates.empty() || dates.size() > 1)
                return {};

            return dates.front();
        }

        std::optional<int> getYear(ISession& session, ReleaseId id, bool original)
        {
            checkReadTransaction(session);

            const char* field{ original ? "original_date" : "date" };
            auto query{ getDboSession(session)->query<core::PartialDateTime>(std::string{ "SELECT " } + "t." + field + " FROM track t").where("t.release_id = ?").bind(id).groupBy(field) };

            bool valid{ true };
            std::optional<int> year{};
            utils::forEachQueryResult(query, [&](core::PartialDateTime dateTime) {
                if (!dateTime.isValid())
                    valid = false;
                else if (!year)
                    year = dateTime.getYear();
                else if (year != dateTime.getYear().value())
                    valid = false;
            });

            if (!year)
                valid = false;

            if (!valid)
                return std::nullopt;

            assert(year);
            return *year;
        }
    } // namespace

    core::PartialDateTime Release::getDate(ISession& session, ReleaseId id)
    {
        return db::getDate(session, id, false);
    }

    core::PartialDateTime Release::getOriginalDate(ISession& session, ReleaseId id)
    {
        return db::getDate(session, id, true);
    }

    std::optional<int> Release::getYear(ISession& session, ReleaseId id)
    {
        return db::getYear(session, id, false);
    }

    std::optional<int> Release::getOriginalYear(ISession& session, ReleaseId id)
    {
        return db::getYear(session, id, true);
    }

    bool Release::hasVariousCopyrights(ISession& session, ReleaseId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(DISTINCT copyright) FROM track t").where("t.release_id = ?").bind(id)) > 1;
    }

    std::optional<std::string> Release::getCopyright(ISession& session, ReleaseId id)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<std::string>("SELECT copyright FROM track t INNER JOIN release r ON r.id = t.release_id").where("r.id = ?").groupBy("copyright").bind(id) };

        auto copyrights{ utils::fetchQueryResults(query) };

        // various copyrights => no copyright
        if (copyrights.empty() || copyrights.size() > 1 || copyrights.front().empty())
            return std::nullopt;

        return std::move(copyrights.front());
    }

    std::optional<std::string> Release::getCopyrightURL(ISession& session, ReleaseId id)
    {
        checkReadTransaction(session);

        const auto query{ getDboSession(session)->query<std::string>("SELECT copyright_url FROM track t INNER JOIN release r ON r.id = t.release_id").where("r.id = ?").bind(id).groupBy("copyright_url") };

        auto copyrights{ utils::fetchQueryResults(query) };

        // various copyright URLs => no copyright URL
        if (copyrights.empty() || copyrights.size() > 1 || copyrights.front().empty())
            return std::nullopt;

        return std::move(copyrights.front());
    }

    std::size_t Release::getMeanBitrate(ISession& session, ReleaseId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COALESCE(AVG(t.bitrate), 0) FROM track t").where("release_id = ?").bind(id).where("bitrate > 0"));
    }

    std::vector<core::media::Codec> Release::getCodecs(ISession& session, ReleaseId id)
    {
        checkReadTransaction(session);

        // Get the codec ordered by frequency
        auto query{ getDboSession(session)->query<detail::Codec>("SELECT t.codec FROM track t").where("release_id = ?").bind(id).groupBy("t.codec").orderBy("COUNT(t.id) DESC") };

        std::vector<core::media::Codec> res;
        utils::forEachQueryResult(query, [&](detail::Codec codec) {
            if (const auto mediaCodecType{ detail::getMediaCodecType(codec) })
                res.push_back(*mediaCodecType);
        });

        return res;
    }

    std::chrono::milliseconds Release::getDuration(ISession& session, ReleaseId id)
    {
        checkReadTransaction(session);

        using milli = std::chrono::duration<int, std::milli>;

        return utils::fetchQuerySingleResult(getDboSession(session)->query<milli>("SELECT COALESCE(SUM(duration), 0) FROM track t").where("t.release_id = ?").bind(id));
    }

    Wt::WDateTime Release::getAddedTime(ISession& session, ReleaseId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<Wt::WDateTime>("SELECT MAX(file_added) FROM track t").where("t.release_id = ?").bind(id));
    }

    Wt::WDateTime Release::getLastWrittenTime(ISession& session, ReleaseId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<Wt::WDateTime>("SELECT MAX(file_last_write) FROM track t").where("t.release_id = ?").bind(id));
    }

    std::size_t Release::getTrackCount(ISession& session, ReleaseId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(t.id) FROM track t INNER JOIN release r ON r.id = t.release_id").where("r.id = ?").bind(id));
    }

    core::EnumSet<Advisory> Release::getAdvisories(ISession& session, ReleaseId id)
    {
        checkReadTransaction(session);

        core::EnumSet<Advisory> res;

        auto query{ getDboSession(session)->query<Advisory>("SELECT DISTINCT advisory FROM track t").where("t.release_id = ?").bind(id) };
        utils::forEachQueryResult(query, [&](Advisory advisory) {
            res.insert(advisory);
        });

        return res;
    }

    void Release::update(ISession& session, const Release& release)
    {
        utils::updateObject<dbo::Release>(session, release);
    }

    void Release::updatePreferredArtwork(ISession& session, ReleaseId releaseId, ArtworkId artworkId)
    {
        dbo::Release& release{ utils::modifyObject<dbo::Release>(session, releaseId) };
        setRelation(*getDboSession(session), release._preferredArtwork, artworkId);
    }

    void Release::clearArtistLinks(ISession& session, ReleaseId id)
    {
        utils::modifyObject<dbo::Release>(session, id)._releaseArtistLinks.clear();
    }

    void Release::clearLabels(ISession& session, ReleaseId id)
    {
        utils::modifyObject<dbo::Release>(session, id)._labels.clear();
    }

    void Release::clearCountries(ISession& session, ReleaseId id)
    {
        utils::modifyObject<dbo::Release>(session, id)._countries.clear();
    }

    void Release::clearReleaseTypes(ISession& session, ReleaseId id)
    {
        utils::modifyObject<dbo::Release>(session, id)._releaseTypes.clear();
    }

    void Release::addLabel(ISession& session, ReleaseId id, LabelId labelId)
    {
        utils::modifyObject<dbo::Release>(session, id)._labels.insert(getDboSession(session)->loadLazy<dbo::Label>(labelId.getValue()));
    }

    void Release::addCountry(ISession& session, ReleaseId id, CountryId countryId)
    {
        utils::modifyObject<dbo::Release>(session, id)._countries.insert(getDboSession(session)->loadLazy<dbo::Country>(countryId.getValue()));
    }

    void Release::addReleaseType(ISession& session, ReleaseId id, ReleaseTypeId releaseTypeId)
    {
        utils::modifyObject<dbo::Release>(session, id)._releaseTypes.insert(getDboSession(session)->loadLazy<dbo::ReleaseType>(releaseTypeId.getValue()));
    }
} // namespace lms::db
