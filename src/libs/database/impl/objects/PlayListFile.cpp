/*
 * Copyright (C) 2024 Emeric Poupon
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

#include "database/objects/PlayListFile.hpp"

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "core/ILogger.hpp"
#include "core/String.hpp"

#include "ArtworkDbo.hpp"
#include "DboAccess.hpp"
#include "DirectoryDbo.hpp"
#include "MediaLibraryDbo.hpp"
#include "PlayListFileDbo.hpp"
#include "Session.hpp"
#include "TrackDbo.hpp"
#include "TrackListDbo.hpp"
#include "UserDbo.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/PathTraits.hpp"
#include "traits/StringViewTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::PlayListFile)

namespace lms::db
{
    PlayListFile::PlayListFile(const std::filesystem::path& file)
    {
        setAbsoluteFilePath(file);
    }

    PlayListFile PlayListFile::create(ISession& session, const std::filesystem::path& file)
    {
        return utils::addObject(session, std::unique_ptr<dbo::PlayListFile>{ new dbo::PlayListFile{ file } });
    }

    void PlayListFile::destroy(ISession& session, std::span<const PlayListFileId> ids)
    {
        utils::destroyObjects<dbo::PlayListFile>(session, ids);
    }

    std::size_t PlayListFile::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM playlist_file"));
    }

    std::optional<PlayListFile> PlayListFile::find(ISession& session, const std::filesystem::path& p)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleObject(getDboSession(session)->query<Wt::Dbo::ptr<dbo::PlayListFile>>("SELECT pl_f from playlist_file pl_f").where("pl_f.absolute_file_path = ?").bind(p));
    }

    void PlayListFile::find(ISession& session, PlayListFileId& lastRetrievedId, std::size_t count, const std::function<void(const PlayListFile&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::PlayListFile>>("SELECT pl_f from playlist_file pl_f").orderBy("pl_f.id").where("pl_f.id > ?").bind(lastRetrievedId).limit(static_cast<int>(count)) };

        utils::forEachQueryObject(query, [&](const PlayListFile& playList) {
            func(playList);
            lastRetrievedId = playList.getId();
        });
    }

    void PlayListFile::findAbsoluteFilePath(ISession& session, PlayListFileId& lastRetrievedId, std::size_t count, const std::function<void(PlayListFileId playListFileId, const std::filesystem::path& absoluteFilePath)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<std::tuple<PlayListFileId, std::filesystem::path>>("SELECT pl_f.id, pl_f.absolute_file_path FROM playlist_file pl_f").orderBy("pl_f.id").where("pl_f.id > ?").bind(lastRetrievedId).limit(static_cast<int>(count)) };

        utils::forEachQueryResult(query, [&](const auto& res) {
            func(std::get<0>(res), std::get<1>(res));
            lastRetrievedId = std::get<0>(res);
        });
    }

    void PlayListFile::find(ISession& session, const IdRange<PlayListFileId>& idRange, const std::function<void(const PlayListFile&)>& func)
    {
        checkReadTransaction(session);

        assert(idRange.isValid());

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::PlayListFile>>("SELECT pl_f from playlist_file pl_f").orderBy("pl_f.id").where("pl_f.id BETWEEN ? AND ?").bind(idRange.first).bind(idRange.last) };

        utils::forEachQueryObject(query, func);
    }

    IdRange<PlayListFileId> PlayListFile::findNextIdRange(ISession& session, PlayListFileId lastRetrievedId, std::size_t count)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<std::tuple<PlayListFileId, PlayListFileId>>("SELECT MIN(sub.id) AS first_id, MAX(sub.id) AS last_id FROM (SELECT pl_f.id FROM playlist_file pl_f WHERE pl_f.id > ? ORDER BY pl_f.id LIMIT ?) sub") };
        query.bind(lastRetrievedId);
        query.bind(static_cast<int>(count));

        auto res{ utils::fetchQuerySingleResult(query) };
        return IdRange<PlayListFileId>{ .first = std::get<0>(res), .last = std::get<1>(res) };
    }

    std::optional<PlayListFile> PlayListFile::find(ISession& session, PlayListFileId id)
    {
        checkReadTransaction(session);

        if (!id.isValid())
            return std::nullopt;

        return utils::fetchQuerySingleObject(getDboSession(session)->query<Wt::Dbo::ptr<dbo::PlayListFile>>("SELECT pl_f from playlist_file pl_f").where("pl_f.id = ?").bind(id));
    }

    std::vector<std::filesystem::path> PlayListFile::getFiles() const
    {
        std::vector<std::filesystem::path> files;

        try
        {
            Wt::Json::Object root;
            Wt::Json::parse(_entries, root);

            assert(root.type("files") == Wt::Json::Type::Array);
            const Wt::Json::Array& filesArray = root.get("files");
            for (const Wt::Json::Value& file : filesArray)
                files.push_back(static_cast<std::string>(file.toString()));
        }
        catch (const Wt::Json::ParseError& e)
        {
            LMS_LOG(DB, ERROR, "Cannot parse files saved in " << _absoluteFilePath << ": corrupted data?");
        }

        return files;
    }

    void PlayListFile::setAbsoluteFilePath(const std::filesystem::path& filePath)
    {
        assert(filePath.is_absolute());
        _absoluteFilePath = filePath;
        _fileStem = filePath.stem();
    }

    void PlayListFile::setName(std::string_view name)
    {
        _name = core::stringUtils::utf8Truncate(name, _maxNameLength);
        LMS_LOG_IF(DB, WARNING, name.size() > _maxNameLength, "PlaylistFile name too long, truncated to '" << _name << "'");
    }

    void PlayListFile::setFiles(std::span<const std::filesystem::path> files)
    {
        Wt::Json::Object root;

        Wt::Json::Array fileArray;
        for (const auto& file : files)
            fileArray.push_back(Wt::Json::Value{ file.string() });

        root["files"] = std::move(fileArray);
        _entries = Wt::Json::serialize(root);
    }

    void PlayListFile::setCoverImageFile(const std::filesystem::path& file)
    {
        _coverImageFile = file;
    }

    void PlayListFile::updatePreferredArtwork(ISession& session, PlayListFileId id, ArtworkId artworkId)
    {
        dbo::PlayListFile& playListFile{ utils::modifyObject<dbo::PlayListFile>(session, id) };
        setRelation(*getDboSession(session), playListFile._preferredArtwork, artworkId);
    }

    void dbo::PlayListFile::applyIds()
    {
        setRelation(*session(), _mediaLibrary, _mediaLibraryId);
        setRelation(*session(), _directory, _directoryId);
    }

    void PlayListFile::update(ISession& session, const PlayListFile& playListFile)
    {
        utils::updateObject<dbo::PlayListFile>(session, playListFile);
    }
} // namespace lms::db
