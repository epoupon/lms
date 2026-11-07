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
#include "database/objects/ReleaseArtistLink.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/TrackEmbeddedImage.hpp"
#include "database/objects/TrackEmbeddedImageLink.hpp"
#include "database/objects/TrackLyrics.hpp"
#include "database/objects/User.hpp"
#include "database/objects/Work.hpp"

#include "JoinSet.hpp"
#include "Session.hpp"
#include "Utils.hpp"
#include "detail/Types.hpp"
#include "objects/FilterExprCompiler.hpp"
#include "traits/EnumSetTraits.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/PartialDateTimeTraits.hpp"
#include "traits/StringViewTraits.hpp"
#include "traits/UUIDTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::Country)
DBO_INSTANTIATE_TEMPLATES(lms::db::Label)
DBO_INSTANTIATE_TEMPLATES(lms::db::Release)
DBO_INSTANTIATE_TEMPLATES(lms::db::ReleaseType)

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

        template<typename ResultType>
        Wt::Dbo::Query<ResultType> createTrackArtistQuery(Wt::Dbo::Session& session, std::string_view itemToSelect, ReleaseId releaseId, TrackArtistLinkType linkType)
        {
            auto query{ session.query<ResultType>("SELECT " + std::string{ itemToSelect } + " from artist a")
                            .join("track_artist_link t_a_l ON t_a_l.artist_id = a.id")
                            .join("track t ON t.id = t_a_l.track_id")
                            .where("t.release_id = ?")
                            .bind(releaseId)
                            .where("t_a_l.type = ?")
                            .bind(linkType)
                            .groupBy("a.id")
                            .orderBy("t_a_l.id") };

            return query;
        };

    } // namespace

    Country::Country(std::string_view name)
        : _name{ name }
    {
        // As we use the name to uniquely identoify release type, we must throw (and not truncate)
        if (name.size() > _maxNameLength)
            throw Exception{ "Country name is too long: '" + std::string{ name } + "'" };
    }

    Country::pointer Country::create(ISession& session, std::string_view name)
    {
        return utils::addObject(session, std::unique_ptr<Country>{ new Country{ name } });
    }

    void Country::destroy(ISession& session, std::span<const CountryId> ids)
    {
        utils::destroyObjects<Country>(session, ids);
    }

    std::size_t Country::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM country"));
    }

    Country::pointer Country::find(ISession& session, CountryId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<Wt::Dbo::ptr<Country>>("SELECT c from country c").where("c.id = ?").bind(id));
    }

    Country::pointer Country::find(ISession& session, std::string_view name)
    {
        checkReadTransaction(session);

        if (name.size() > _maxNameLength)
            throw Exception{ "Country name is too long: '" + std::string{ name } + "'" };

        return utils::fetchQuerySingleResult(getDboSession(session)->query<Wt::Dbo::ptr<Country>>("SELECT c from country c").where("c.name = ?").bind(name));
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

    Label::pointer Label::create(ISession& session, std::string_view name)
    {
        return utils::addObject(session, std::unique_ptr<Label>{ new Label{ name } });
    }

    void Label::destroy(ISession& session, std::span<const LabelId> ids)
    {
        utils::destroyObjects<Label>(session, ids);
    }

    std::size_t Label::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM label"));
    }

    Label::pointer Label::find(ISession& session, LabelId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<Wt::Dbo::ptr<Label>>("SELECT l from label l").where("l.id = ?").bind(id));
    }

    Label::pointer Label::find(ISession& session, std::string_view name)
    {
        checkReadTransaction(session);

        if (name.size() > _maxNameLength)
            throw Exception{ "Label name is too long: '" + std::string{ name } + "'" };

        return utils::fetchQuerySingleResult(getDboSession(session)->query<Wt::Dbo::ptr<Label>>("SELECT l from label l").where("l.name = ?").bind(name));
    }

    void Label::find(ISession& session, LabelSortMethod sortMethod, std::function<void(const Label::pointer& label)> func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->find<Label>() };
        switch (sortMethod)
        {
        case LabelSortMethod::None:
            break;
        case LabelSortMethod::Name:
            query.orderBy("name COLLATE NOCASE");
        }

        utils::forEachQueryResult(query, [&](const Label::pointer& label) {
            func(label);
        });
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

    ReleaseType::pointer ReleaseType::create(ISession& session, std::string_view name)
    {
        return utils::addObject(session, std::unique_ptr<ReleaseType>{ new ReleaseType{ name } });
    }

    void ReleaseType::destroy(ISession& session, std::span<const ReleaseTypeId> ids)
    {
        utils::destroyObjects<ReleaseType>(session, ids);
    }

    std::size_t ReleaseType::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM release_type"));
    }

    ReleaseType::pointer ReleaseType::find(ISession& session, ReleaseTypeId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<Wt::Dbo::ptr<ReleaseType>>("SELECT r_t from release_type r_t").where("r_t.id = ?").bind(id));
    }

    ReleaseType::pointer ReleaseType::find(ISession& session, std::string_view name)
    {
        checkReadTransaction(session);

        if (name.size() > _maxNameLength)
            throw Exception{ "ReleaseType is too long: '" + std::string{ name } + "'" };

        return utils::fetchQuerySingleResult(getDboSession(session)->query<Wt::Dbo::ptr<ReleaseType>>("SELECT r_t from release_type r_t").where("r_t.name = ?").bind(name));
    }

    void ReleaseType::find(ISession& session, ReleaseTypeSortMethod sortMethod, std::function<void(const ReleaseType::pointer& releaseType)> func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->find<ReleaseType>() };
        switch (sortMethod)
        {
        case ReleaseTypeSortMethod::None:
            break;
        case ReleaseTypeSortMethod::Name:
            query.orderBy("name COLLATE NOCASE");
        }

        utils::forEachQueryResult(query, [&](const ReleaseType::pointer& releaseType) {
            func(releaseType);
        });
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

    Release::pointer Release::create(ISession& session, const std::string& name, const std::optional<core::UUID>& MBID)
    {
        return utils::addObject(session, std::unique_ptr<Release>{ new Release{ name, MBID } });
    }

    void Release::destroy(ISession& session, std::span<const ReleaseId> ids)
    {
        utils::destroyObjects<Release>(session, ids);
    }

    Release::pointer Release::find(ISession& session, const core::UUID& mbid)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<Wt::Dbo::ptr<Release>>("SELECT r from release r").where("r.mbid = ?").bind(mbid));
    }

    Release::pointer Release::find(ISession& session, ReleaseId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<Wt::Dbo::ptr<Release>>("SELECT r from release r").where("r.id = ?").bind(id));
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

    void Release::find(ISession& session, ReleaseId& lastRetrievedRelease, std::size_t count, const std::function<void(const Release::pointer&)>& func, MediaLibraryId library)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<Release>>("SELECT r FROM release r").orderBy("r.id").where("r.id > ?").bind(lastRetrievedRelease).limit(static_cast<int>(count)) };

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

    void Release::find(ISession& session, const IdRange<ReleaseId>& idRange, const std::function<void(const Release::pointer&)>& func)
    {
        checkReadTransaction(session);

        assert(idRange.isValid());

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<Release>>("SELECT r from release r").orderBy("r.id").where("r.id BETWEEN ? AND ?").bind(idRange.first).bind(idRange.last) };

        utils::forEachQueryResult(query, [&](const Release::pointer& release) {
            func(release);
        });
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

    std::vector<Release::pointer> Release::find(ISession& session, const FindParameters& params)
    {
        checkReadTransaction(session);

        auto query{ createListQuery<Wt::Dbo::ptr<Release>>(session, "r", params) };
        return utils::execRangeQuery<pointer>(query, params.range);
    }

    void Release::find(ISession& session, const FindParameters& params, const std::function<void(const pointer&)>& func)
    {
        checkReadTransaction(session);

        auto query{ createListQuery<Wt::Dbo::ptr<Release>>(session, "r", params) };
        utils::forEachQueryRangeResult(query, params.range, func);
    }

    std::vector<ReleaseId> Release::findIds(ISession& session, const FindParameters& params)
    {
        checkReadTransaction(session);

        auto query{ createListQuery<ReleaseId>(session, "r.id", params) };
        return utils::execRangeQuery<ReleaseId>(query, params.range);
    }

    void Release::updatePreferredArtwork(ISession& session, ReleaseId releaseId, ArtworkId artworkId)
    {
        checkWriteTransaction(session);

        if (artworkId.isValid())
            utils::executeCommand(*getDboSession(session), "UPDATE release SET preferred_artwork_id = ? WHERE id = ?", artworkId, releaseId);
        else
            utils::executeCommand(*getDboSession(session), "UPDATE release SET preferred_artwork_id = NULL WHERE id = ?", releaseId);
    }

    std::size_t Release::getCount(ISession& session, const FindParameters& params)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(createCountQuery(session, params));
    }

    core::PartialDateTime Release::getDate() const
    {
        return getDate(false);
    }

    core::PartialDateTime Release::getOriginalDate() const
    {
        return getDate(true);
    }

    core::PartialDateTime Release::getDate(bool original) const
    {
        assert(session());

        const char* field{ original ? "original_date" : "date" };
        auto query{ (session()->query<core::PartialDateTime>(std::string{ "SELECT " } + "t." + field + " FROM track t").where("t.release_id = ?").groupBy(field).bind(getId())) };

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

        const char* field{ original ? "original_date" : "date" };
        auto query{ session()->query<core::PartialDateTime>(std::string{ "SELECT " } + "t." + field + " FROM track t").where("t.release_id = ?").bind(getId()).groupBy(field) };

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

    bool Release::hasVariousCopyrights() const
    {
        assert(session());

        return utils::fetchQuerySingleResult(session()->query<int>("SELECT COUNT(DISTINCT copyright) FROM track t").where("t.release_id = ?").bind(getId())) > 1;
    }

    std::optional<std::string> Release::getCopyright() const
    {
        assert(session());

        auto query{ session()->query<std::string>("SELECT copyright FROM track t INNER JOIN release r ON r.id = t.release_id").where("r.id = ?").groupBy("copyright").bind(getId()) };

        auto copyrights{ utils::fetchQueryResults(query) };

        // various copyrights => no copyright
        if (copyrights.empty() || copyrights.size() > 1 || copyrights.front().empty())
            return std::nullopt;

        return std::move(copyrights.front());
    }

    std::optional<std::string> Release::getCopyrightURL() const
    {
        assert(session());

        const auto query{ session()->query<std::string>("SELECT copyright_url FROM track t INNER JOIN release r ON r.id = t.release_id").where("r.id = ?").bind(getId()).groupBy("copyright_url") };

        auto copyrights{ utils::fetchQueryResults(query) };

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

    std::vector<core::media::Codec> Release::getCodecs() const
    {
        assert(session());

        // Get the codec ordered by frequency
        auto query{ session()->query<detail::Codec>("SELECT t.codec FROM track t").where("release_id = ?").bind(getId()).groupBy("t.codec").orderBy("COUNT(t.id) DESC") };

        std::vector<core::media::Codec> res;
        utils::forEachQueryResult(query, [&](detail::Codec codec) {
            if (const auto mediaCodecType{ detail::getMediaCodecType(codec) })
                res.push_back(*mediaCodecType);
        });

        return res;
    }

    std::vector<ObjectPtr<Artist>> Release::getArtists() const
    {
        assert(session());

        auto query{ session()->query<Wt::Dbo::ptr<Artist>>("SELECT a from artist a") };
        query.join("release_artist_link r_a_l ON r_a_l.artist_id = a.id");
        query.where("r_a_l.release_id = ?").bind(getId());
        query.groupBy("a.id");

        return utils::fetchQueryResults<Artist::pointer>(query);
    }

    bool Release::hasArtist(ArtistId artistId) const
    {
        assert(session());

        auto query{ session()->query<int>("SELECT COUNT(1) FROM release_artist_link r_a_l") };
        query.where("r_a_l.release_id = ?").bind(getId());
        query.where("r_a_l.artist_id = ?").bind(artistId);

        return utils::fetchQuerySingleResult(query) > 0;
    }

    std::vector<Artist::pointer> Release::getTrackArtists(TrackArtistLinkType type) const
    {
        assert(session());

        const auto query{ createTrackArtistQuery<Wt::Dbo::ptr<Artist>>(*session(), "a", getId(), type) };
        return utils::fetchQueryResults<Artist::pointer>(query);
    }

    void Release::visitTrackArtists(TrackArtistLinkType type, std::function<void(const ObjectPtr<Artist>&)> visitor) const
    {
        assert(session());

        const auto query{ createTrackArtistQuery<Wt::Dbo::ptr<Artist>>(*session(), "a", getId(), type) };
        return utils::forEachQueryResult(query, visitor);
    }

    std::vector<ArtistId> Release::getTrackArtistIds(TrackArtistLinkType linkType) const
    {
        assert(session());

        const auto query{ createTrackArtistQuery<ArtistId>(*session(), "a.id", getId(), linkType) };
        return utils::fetchQueryResults(query);
    }

    ObjectPtr<Artwork> Release::getPreferredArtwork() const
    {
        return ObjectPtr<Artwork>{ _preferredArtwork };
    }

    ArtworkId Release::getPreferredArtworkId() const
    {
        return _preferredArtwork.id();
    }

    std::vector<ObjectPtr<Medium>> Release::getMediums() const
    {
        assert(session());

        // Select the similar releases using the 5 most used clusters of the release
        auto query{ session()->query<Wt::Dbo::ptr<Medium>>("SELECT m from medium m") };
        query.where("m.release_id = ?").bind(getId());
        query.orderBy("m.position");

        return utils::fetchQueryResults<Medium::pointer>(query);
    }

    std::vector<ObjectPtr<ReleaseArtistLink>> Release::getArtistLinks() const
    {
        return utils::fetchQueryResults<ReleaseArtistLink::pointer>(_releaseArtistLinks.find());
    }

    void Release::visitArtistLinks(const std::function<void(const ReleaseArtistLink::pointer& artistLink)>& visitor) const
    {
        return utils::forEachQueryResult(_releaseArtistLinks.find(), visitor);
    }

    void Release::visitTrackArtistLinks(TrackArtistLinkType linkType, const std::function<void(const ObjectPtr<TrackArtistLink>& artistLink)>& visitor) const
    {
        auto query{ session()->query<Wt::Dbo::ptr<TrackArtistLink>>("SELECT t_a_l from track_artist_link t_a_l") };
        query.join("track t ON t.id = t_a_l.track_id");
        query.where("t.release_id = ?").bind(getId());
        query.where("t_a_l.type = ?").bind(linkType);

        utils::forEachQueryResult(query, visitor);
    }

    void Release::clearArtistLinks()
    {
        _releaseArtistLinks.clear();
    }

    void Release::addArtistLink(const ObjectPtr<ReleaseArtistLink>& artistLink)
    {
        _releaseArtistLinks.insert(getDboPtr(artistLink));
    }

    void Release::clearLabels()
    {
        _labels.clear();
    }

    void Release::clearCountries()
    {
        _countries.clear();
    }

    void Release::clearReleaseTypes()
    {
        _releaseTypes.clear();
    }

    void Release::addLabel(const ObjectPtr<Label>& label)
    {
        _labels.insert(getDboPtr(label));
    }

    void Release::addCountry(const ObjectPtr<Country>& country)
    {
        _countries.insert(getDboPtr(country));
    }

    void Release::addReleaseType(const ObjectPtr<ReleaseType>& releaseType)
    {
        _releaseTypes.insert(getDboPtr(releaseType));
    }

    void Release::setPreferredArtwork(const ObjectPtr<Artwork>& artwork)
    {
        _preferredArtwork = getDboPtr(artwork);
    }

    bool Release::hasVariousArtists() const
    {
        // TODO optimize
        return getTrackArtists().size() > 1;
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

    std::vector<std::string> Release::getLabelNames() const
    {
        std::vector<std::string> res;

        auto query{ _labels.find() };
        utils::forEachQueryResult(query, [&](const Label::pointer& label) {
            res.push_back(std::string{ label->getName() });
        });

        return res;
    }

    std::vector<std::string> Release::getCountryNames() const
    {
        std::vector<std::string> res;

        auto query{ _countries.find() };
        utils::forEachQueryResult(query, [&](const Country::pointer& country) {
            res.push_back(std::string{ country->getName() });
        });

        return res;
    }

    std::vector<std::string> Release::getReleaseTypeNames() const
    {
        std::vector<std::string> res;

        auto query{ _releaseTypes.find() };
        utils::forEachQueryResult(query, [&](const ReleaseType::pointer& releaseType) {
            res.push_back(std::string{ releaseType->getName() });
        });

        return res;
    }

    void Release::visitLabels(const std::function<void(const Label::pointer& label)>& _func) const
    {
        assert(session());
        auto query{ _labels.find() };
        utils::forEachQueryResult(query, _func);
    }

    core::EnumSet<Advisory> Release::getAdvisories() const
    {
        core::EnumSet<Advisory> res;

        auto query{ session()->query<Advisory>("SELECT DISTINCT advisory FROM track t").where("t.release_id = ?").bind(getId()) };

        utils::forEachQueryResult(query, [&](Advisory advisory) {
            res.insert(advisory);
        });

        return res;
    }

    std::chrono::milliseconds Release::getDuration() const
    {
        assert(session());

        using milli = std::chrono::duration<int, std::milli>;

        return utils::fetchQuerySingleResult(session()->query<milli>("SELECT COALESCE(SUM(duration), 0) FROM track t").where("t.release_id = ?").bind(getId()));
    }

    Wt::WDateTime Release::getLastWrittenTime() const
    {
        assert(session());

        return utils::fetchQuerySingleResult(session()->query<Wt::WDateTime>("SELECT MAX(file_last_write) FROM track t").where("t.release_id = ?").bind(getId()));
    }

    Wt::WDateTime Release::getAddedTime() const
    {
        assert(session());

        return utils::fetchQuerySingleResult(session()->query<Wt::WDateTime>("SELECT MAX(file_added) FROM track t").where("t.release_id = ?").bind(getId()));
    }

    std::vector<ObjectPtr<Cluster>> Release::getClusters(ClusterTypeId clusterTypeId, std::size_t maxCount) const
    {
        assert(session());

        auto query{ session()->query<Wt::Dbo::ptr<Cluster>>("SELECT c FROM cluster c INNER JOIN track_cluster t_c ON t_c.cluster_id = c.id INNER JOIN track t ON t.id = t_c.track_id") };
        query.where("t.release_id = ?").bind(getId());
        query.where("c.cluster_type_id = ?").bind(clusterTypeId);
        query.groupBy("c.id");
        query.orderBy("COUNT(c.id) DESC");
        query.limit(static_cast<int>(maxCount));

        std::vector<ObjectPtr<Cluster>> res;
        utils::forEachQueryResult(query, [&](const Wt::Dbo::ptr<Cluster>& cluster) {
            res.push_back(cluster);
        });

        return res;
    }
} // namespace lms::db
