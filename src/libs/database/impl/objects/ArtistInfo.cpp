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

#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/ArtistInfoId.hpp"
#include "database/objects/Directory.hpp"

#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/PathTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::ArtistInfo)

namespace lms::db
{
    ArtistInfo::pointer ArtistInfo::create(Session& session)
    {
        return session.getDboSession()->add(std::unique_ptr<ArtistInfo>{ new ArtistInfo{} });
    }

    std::size_t ArtistInfo::getCount(Session& session)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM artist_info"));
    }

    ArtistInfo::pointer ArtistInfo::find(Session& session, const std::filesystem::path& p)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<ArtistInfo>>("SELECT a_i from artist_info a_i").where("a_i.absolute_file_path = ?").bind(p));
    }

    ArtistInfo::pointer ArtistInfo::find(Session& session, ArtistInfoId id)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<ArtistInfo>>("SELECT a_i from artist_info a_i").where("a_i.id = ?").bind(id));
    }

    void ArtistInfo::find(Session& session, ArtistId id, std::optional<Range> range, const std::function<void(const pointer&)>& func)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<ArtistInfo>>("SELECT a_i from artist_info a_i").where("a_i.artist_id = ?").bind(id) };
        utils::forEachQueryRangeResult(query, range, [&](const ArtistInfo::pointer& entry) {
            func(entry);
        });
    }

    void ArtistInfo::find(Session& session, ArtistId id, const std::function<void(const pointer&)>& func)
    {
        find(session, id, std::nullopt, func);
    }

    void ArtistInfo::find(Session& session, ArtistInfoId& lastRetrievedId, std::size_t count, const std::function<void(const pointer&)>& func)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<ArtistInfo>>("SELECT a_i from artist_info a_i").orderBy("a_i.id").where("a_i.id > ?").bind(lastRetrievedId).limit(static_cast<int>(count)) };

        utils::forEachQueryResult(query, [&](const ArtistInfo::pointer& entry) {
            func(entry);
            lastRetrievedId = entry->getId();
        });
    }

    void ArtistInfo::findArtistNameNoLongerMatch(Session& session, std::optional<Range> range, const std::function<void(const pointer&)>& func)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<ArtistInfo>>("SELECT a_i FROM artist_info a_i") };
        query.join("artist a ON a_i.artist_id = a.id");
        query.where("a_i.mbid_matched = FALSE");
        query.where("a_i.name <> a.name");

        utils::applyRange(query, range);
        utils::forEachQueryResult(query, [&](const pointer& info) {
            func(info);
        });
    }

    void ArtistInfo::findWithArtistNameAmbiguity(Session& session, std::optional<Range> range, bool allowArtistMBIDFallback, const std::function<void(const pointer&)>& func)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<ArtistInfo>>("SELECT a_i FROM artist_info a_i") };
        query.join("artist a ON a_i.artist_id = a.id");
        query.where("a_i.mbid_matched = FALSE");
        if (!allowArtistMBIDFallback)
        {
            query.where("a.mbid <> ''");
        }
        else
        {
            query.where(R"(
                (a.mbid <> '' AND EXISTS (SELECT 1 FROM artist a2 WHERE a2.name = a.name AND a2.mbid <> '' AND a2.mbid <> a.mbid))
                OR (a.mbid = '' AND (SELECT COUNT(*) FROM artist a2 WHERE a2.name = a.name AND a2.mbid <> '') = 1))");
        }

        utils::applyRange(query, range);
        utils::forEachQueryResult(query, [&](const pointer& info) {
            func(info);
        });
    }

    void ArtistInfo::findAbsoluteFilePath(Session& session, ArtistInfoId& lastRetrievedId, std::size_t count, const std::function<void(ArtistInfoId artistInfoId, const std::filesystem::path& absoluteFilePath)>& func)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<std::tuple<ArtistInfoId, std::filesystem::path>>("SELECT a_i.id, a_i.absolute_file_path FROM artist_info a_i").orderBy("a_i.id").where("a_i.id > ?").bind(lastRetrievedId).limit(static_cast<int>(count)) };

        utils::forEachQueryResult(query, [&](const auto& res) {
            func(std::get<0>(res), std::get<1>(res));
            lastRetrievedId = std::get<0>(res);
        });
    }

    Artist::pointer ArtistInfo::getArtist() const
    {
        return _artist;
    }

    Directory::pointer ArtistInfo::getDirectory() const
    {
        return _directory;
    }

    void ArtistInfo::setAbsoluteFilePath(const std::filesystem::path& filePath)
    {
        assert(filePath.is_absolute());
        _absoluteFilePath = filePath;
        _fileStem = filePath.stem();
    }

    void ArtistInfo::setDirectory(ObjectPtr<Directory> directory)
    {
        _directory = getDboPtr(directory);
    }

    void ArtistInfo::setArtist(ObjectPtr<Artist> artist)
    {
        _artist = getDboPtr(artist);
    }
} // namespace lms::db
