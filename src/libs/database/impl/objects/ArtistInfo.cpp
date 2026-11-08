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

#include "database/objects/ArtistInfo.hpp"

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "core/ILogger.hpp"
#include "core/String.hpp"

#include "database/objects/ArtistInfoId.hpp"

#include "ArtistDbo.hpp"
#include "ArtistInfoDbo.hpp"
#include "DirectoryDbo.hpp"
#include "ObjectPtrImpl.hpp"
#include "Session.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/PathTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::ArtistInfo)

namespace lms::db
{
    ArtistInfo::pointer ArtistInfo::create(ISession& session)
    {
        return utils::addObject(session, std::unique_ptr<dbo::ArtistInfo>{ new dbo::ArtistInfo{} });
    }

    void ArtistInfo::destroy(ISession& session, std::span<const ArtistInfoId> ids)
    {
        utils::destroyObjects<dbo::ArtistInfo>(session, ids);
    }

    std::size_t ArtistInfo::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM artist_info"));
    }

    ArtistInfo::pointer ArtistInfo::find(ISession& session, const std::filesystem::path& p)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleObject(getDboSession(session)->query<Wt::Dbo::ptr<dbo::ArtistInfo>>("SELECT a_i from artist_info a_i").where("a_i.absolute_file_path = ?").bind(p));
    }

    ArtistInfo::pointer ArtistInfo::find(ISession& session, ArtistInfoId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleObject(getDboSession(session)->query<Wt::Dbo::ptr<dbo::ArtistInfo>>("SELECT a_i from artist_info a_i").where("a_i.id = ?").bind(id));
    }

    void ArtistInfo::find(ISession& session, ArtistId id, std::optional<Range> range, const std::function<void(const pointer&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::ArtistInfo>>("SELECT a_i from artist_info a_i").where("a_i.artist_id = ?").bind(id) };
        utils::forEachQueryRangeObject(query, range, [&](const ArtistInfo::pointer& entry) {
            func(entry);
        });
    }

    void ArtistInfo::find(ISession& session, ArtistId id, const std::function<void(const pointer&)>& func)
    {
        find(session, id, std::nullopt, func);
    }

    void ArtistInfo::find(ISession& session, ArtistInfoId& lastRetrievedId, std::size_t count, const std::function<void(const pointer&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::ArtistInfo>>("SELECT a_i from artist_info a_i").orderBy("a_i.id").where("a_i.id > ?").bind(lastRetrievedId).limit(static_cast<int>(count)) };

        utils::forEachQueryObject(query, [&](const ArtistInfo::pointer& entry) {
            func(entry);
            lastRetrievedId = entry->getId();
        });
    }

    void ArtistInfo::findArtistNameNoLongerMatch(ISession& session, std::optional<Range> range, const std::function<void(const pointer&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::ArtistInfo>>("SELECT a_i FROM artist_info a_i") };
        query.join("artist a ON a_i.artist_id = a.id");
        query.where("a_i.mbid_matched = FALSE");
        query.where("a_i.name <> a.name");

        utils::applyRange(query, range);
        utils::forEachQueryObject(query, [&](const pointer& info) {
            func(info);
        });
    }

    void ArtistInfo::findWithArtistNameAmbiguity(ISession& session, std::optional<Range> range, bool allowArtistMBIDFallback, const std::function<void(const pointer&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::ArtistInfo>>("SELECT a_i FROM artist_info a_i") };
        query.join("artist a ON a_i.artist_id = a.id");
        query.where("a_i.mbid_matched = FALSE");
        if (!allowArtistMBIDFallback)
        {
            query.where("a.mbid IS NOT NULL");
        }
        else
        {
            query.where(R"(
                (a.mbid IS NOT NULL AND EXISTS (SELECT 1 FROM artist a2 WHERE a2.name = a.name AND a2.mbid IS NOT NULL AND a2.mbid <> a.mbid))
                OR (a.mbid IS NULL AND (SELECT COUNT(*) FROM artist a2 WHERE a2.name = a.name AND a2.mbid IS NOT NULL) = 1))");
        }

        utils::applyRange(query, range);
        utils::forEachQueryObject(query, [&](const pointer& info) {
            func(info);
        });
    }

    void ArtistInfo::findAbsoluteFilePath(ISession& session, ArtistInfoId& lastRetrievedId, std::size_t count, const std::function<void(ArtistInfoId artistInfoId, const std::filesystem::path& absoluteFilePath)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<std::tuple<ArtistInfoId, std::filesystem::path>>("SELECT a_i.id, a_i.absolute_file_path FROM artist_info a_i").orderBy("a_i.id").where("a_i.id > ?").bind(lastRetrievedId).limit(static_cast<int>(count)) };

        utils::forEachQueryResult(query, [&](const auto& res) {
            func(std::get<0>(res), std::get<1>(res));
            lastRetrievedId = std::get<0>(res);
        });
    }

    Artist::pointer ArtistInfo::getArtist() const
    {
        return toObjectPtr(getDbo(*this)._artist);
    }

    Directory::pointer ArtistInfo::getDirectory() const
    {
        return toObjectPtr(getDbo(*this)._directory);
    }

    void ArtistInfo::setAbsoluteFilePath(const std::filesystem::path& filePath)
    {
        assert(filePath.is_absolute());
        _absoluteFilePath = filePath;
        _fileStem = filePath.stem();
    }

    void ArtistInfo::setDirectory(const ObjectPtr<Directory>& directory)
    {
        getDbo(*this)._directory = getDboPtr(directory);
    }

    void ArtistInfo::setArtist(const ObjectPtr<Artist>& artist)
    {
        getDbo(*this)._artist = getDboPtr(artist);
    }

    // Truncated as the artist name/sort name, or they would never match
    void ArtistInfo::setName(std::string_view name)
    {
        _name = core::stringUtils::utf8Truncate(name, Artist::maxNameLength);
        LMS_LOG_IF(DB, WARNING, name.size() > Artist::maxNameLength, "Artist info name too long, truncated to '" << _name << "'");
    }

    void ArtistInfo::setSortName(std::string_view sortName)
    {
        _sortName = core::stringUtils::utf8Truncate(sortName, Artist::maxNameLength);
        LMS_LOG_IF(DB, WARNING, sortName.size() > Artist::maxNameLength, "Artist info sort name too long, truncated to '" << _sortName << "'");
    }

    DirectoryId ArtistInfo::getDirectoryId() const
    {
        return getDbo(*this)._directory.id();
    }
} // namespace lms::db
