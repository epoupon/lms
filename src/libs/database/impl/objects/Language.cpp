/*
 * Copyright (C) 2025 Emeric Poupon
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

#include "database/objects/Language.hpp"

#include <Wt/Dbo/Impl.h>

#include "core/ILogger.hpp"
#include "core/String.hpp"
#include "core/Utils.hpp"

#include "ArtistDbo.hpp"
#include "ArtworkDbo.hpp"
#include "ClusterDbo.hpp"
#include "DboAccess.hpp"
#include "DirectoryDbo.hpp"
#include "GenreDbo.hpp"
#include "GroupingDbo.hpp"
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
#include "traits/IdTypeTraits.hpp"
#include "traits/StringViewTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::Language)

namespace lms::db
{
    namespace
    {
        template<typename ResultType>
        Wt::Dbo::Query<ResultType> createQuery(ISession& session, std::string_view itemToSelect, const Language::FindParameters& params)
        {
            checkReadTransaction(session);

            auto query{ getDboSession(session)->query<ResultType>("SELECT " + std::string{ itemToSelect } + " FROM language l") };

            if (params.artist.isValid() || params.track.isValid() || params.release.isValid()
                || params.sortMethod == LanguageSortMethod::TrackCountDesc)
                query.join("track_language t_l ON t_l.language_id = l.id");

            if (params.track.isValid())
                query.where("t_l.track_id = ?").bind(params.track);

            if (params.release.isValid() || params.artist.isValid())
                query.join("track t ON t.id = t_l.track_id");

            if (params.release.isValid())
                query.where("t.release_id = ?").bind(params.release);

            if (params.artist.isValid())
            {
                query.join("track_artist_link t_a_l ON t_a_l.track_id = t.id");
                query.where("t_a_l.artist_id = ?").bind(params.artist);
            }

            switch (params.sortMethod)
            {
            case LanguageSortMethod::None:
                break;
            case LanguageSortMethod::Name:
                query.orderBy("l.name COLLATE NOCASE");
                break;
            case LanguageSortMethod::TrackCountDesc:
                query.orderBy("COUNT(t_l.track_id) DESC");
                break;
            }

            // track_language has a UNIQUE constraint on (track_id, language_id), so no duplicates when filtering by track
            if (!params.track.isValid())
                query.groupBy("l.id");

            return query;
        }

        template<typename ResultType>
        Wt::Dbo::Query<ResultType> createQuery(ISession& session, const Language::FindParameters& params)
        {
            std::string_view itemToSelect;

            if constexpr (std::is_same_v<ResultType, LanguageId>)
                itemToSelect = "l.id";
            else if constexpr (std::is_same_v<ResultType, Wt::Dbo::ptr<dbo::Language>>)
                itemToSelect = "l";
            else
                static_assert(core::utils::dependent_false_v<ResultType>, "Unhandled type");

            return createQuery<ResultType>(session, itemToSelect, params);
        }
    } // namespace

    Language::Language(std::string_view name)
        : _name{ core::stringUtils::utf8Truncate(name, maxNameLength) }
    {
        LMS_LOG_IF(DB, WARNING, name.size() > maxNameLength, "Language name too long, truncated to '" << _name << "'");
    }

    Language Language::create(ISession& session, std::string_view name)
    {
        return utils::addObject(session, std::unique_ptr<dbo::Language>{ new dbo::Language{ name } });
    }

    void Language::destroy(ISession& session, std::span<const LanguageId> ids)
    {
        utils::destroyObjects<dbo::Language>(session, ids);
    }

    std::size_t Language::getCount(ISession& session)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM language"));
    }

    std::vector<LanguageId> Language::findIds(ISession& session, const FindParameters& params)
    {
        checkReadTransaction(session);
        auto query{ createQuery<LanguageId>(session, params) };
        return utils::execRangeQuery<LanguageId>(query, params.range);
    }

    void Language::find(ISession& session, const FindParameters& params, const std::function<void(const Language&)>& func)
    {
        checkReadTransaction(session);
        auto query{ createQuery<Wt::Dbo::ptr<dbo::Language>>(session, params) };
        utils::forEachQueryRangeObject(query, params.range, func);
    }

    std::optional<Language> Language::find(ISession& session, LanguageId id)
    {
        checkReadTransaction(session);

        if (!id.isValid())
            return std::nullopt;

        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::Language>().where("id = ?").bind(id));
    }

    std::optional<Language> Language::find(ISession& session, std::string_view name)
    {
        checkReadTransaction(session);

        name = core::stringUtils::utf8Truncate(name, maxNameLength);

        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::Language>().where("name = ?").bind(name));
    }

    std::vector<LanguageId> Language::findOrphanIds(ISession& session, std::optional<Range> range)
    {
        checkReadTransaction(session);
        auto query{ getDboSession(session)->query<LanguageId>("SELECT l.id FROM language l WHERE NOT EXISTS (SELECT 1 FROM track_language t_l WHERE t_l.language_id = l.id)") };
        return utils::execRangeQuery<LanguageId>(query, range);
    }

} // namespace lms::db
