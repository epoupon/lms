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

#include "database/objects/Directory.hpp"

#include <sstream>

#include <Wt/Dbo/Impl.h>

#include "database/Session.hpp"
#include "database/Types.hpp"
#include "database/objects/Filters.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "database/objects/detail/Types.hpp"

#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/PathTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::Directory)

namespace lms::db
{
    namespace
    {
        Wt::Dbo::Query<Wt::Dbo::ptr<Directory>> createQuery(Session& session, const Directory::FindParameters& params)
        {
            auto query{ session.getDboSession()->query<Wt::Dbo::ptr<Directory>>("SELECT d FROM directory d") };

            for (std::string_view keyword : params.keywords)
                query.where("d.name LIKE ? ESCAPE '" ESCAPE_CHAR_STR "'").bind("%" + utils::escapeForLikeKeyword(keyword) + "%");

            if (params.trackArtist.isValid()
                || params.releaseArtist.isValid()
                || params.release.isValid()
                || params.medium.isValid())
            {
                query.join("track t ON t.directory_id = d.id");
                query.groupBy("d.id");
            }

            if (params.mediaLibrary.isValid())
                query.where("d.media_library_id = ?").bind(params.mediaLibrary);

            if (params.parentDirectory.isValid())
                query.where("d.parent_directory_id = ?").bind(params.parentDirectory);

            if (params.medium.isValid())
                query.where("t.medium_id = ?").bind(params.medium);

            if (params.release.isValid())
                query.where("t.release_id = ?").bind(params.release);

            if (params.releaseArtist.isValid())
            {
                assert(!params.trackArtist.isValid());

                query.join("artist a ON a.id = r_a_l.artist_id")
                    .join("release_artist_link r_a_l ON r_a_l.release_id = t.release_id")
                    .where("a.id = ?")
                    .bind(params.releaseArtist);
            }

            if (params.trackArtist.isValid())
            {
                assert(!params.releaseArtist.isValid());

                query.join("artist a ON a.id = t_a_l.artist_id")
                    .join("track_artist_link t_a_l ON t_a_l.track_id = t.id")
                    .where("a.id = ?")
                    .bind(params.trackArtist);

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
            }

            if (params.withNoTrack)
                query.where("NOT EXISTS (SELECT 1 FROM track t WHERE t.directory_id = d.id)");

            switch (params.sortMethod)
            {
            case DirectorySortMethod::None:
                break;
            case DirectorySortMethod::Name:
                query.orderBy("d.name COLLATE NOCASE");
                break;
            }

            return query;
        }

        std::filesystem::path getPathWithTrailingSeparator(const std::filesystem::path& path)
        {
            if (path.empty())
                return path;

            std::string pathStr{ path.string() };

            // Check if the last character is a directory separator
            if (pathStr.back() != std::filesystem::path::preferred_separator)
                pathStr += std::filesystem::path::preferred_separator;

            return std::filesystem::path{ pathStr };
        }
    } // namespace

    Directory::Directory(const std::filesystem::path& p)
    {
        setAbsolutePath(p);
    }

    Directory::pointer Directory::create(Session& session, const std::filesystem::path& p)
    {
        return session.getDboSession()->add(std::unique_ptr<Directory>{ new Directory{ p } });
    }

    std::size_t Directory::getCount(Session& session)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM directory"));
    }

    Directory::pointer Directory::find(Session& session, DirectoryId id)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<Directory>>("SELECT d from directory d").where("d.id = ?").bind(id));
    }

    Directory::pointer Directory::find(Session& session, const std::filesystem::path& path)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<Directory>>("SELECT d from directory d").where("d.absolute_path = ?").bind(path));
    }

    void Directory::find(Session& session, DirectoryId& lastRetrievedDirectory, std::size_t count, const std::function<void(const Directory::pointer&)>& func)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<Directory>>("SELECT d from directory d").orderBy("d.id").where("d.id > ?").bind(lastRetrievedDirectory).limit(static_cast<int>(count)) };

        utils::forEachQueryResult(query, [&](const Directory::pointer& image) {
            func(image);
            lastRetrievedDirectory = image->getId();
        });
    }

    RangeResults<Directory::pointer> Directory::find(Session& session, const FindParameters& params)
    {
        auto query{ createQuery(session, params) };
        return utils::execRangeQuery<Directory::pointer>(query, params.range);
    }

    void Directory::find(Session& session, const FindParameters& params, const std::function<void(const Directory::pointer&)>& func)
    {
        auto query{ createQuery(session, params) };
        utils::forEachQueryRangeResult(query, params.range, [&func](const Directory::pointer& dir) {
            func(dir);
        });
    }

    RangeResults<DirectoryId> Directory::findOrphanIds(Session& session, std::optional<Range> range)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<DirectoryId>("SELECT d.id FROM directory d") };
        query.leftJoin("directory d_child ON d.id = d_child.parent_directory_id");
        query.leftJoin("track t ON d.id = t.directory_id");
        query.leftJoin("image i ON d.id = i.directory_id");
        query.leftJoin("track_lyrics l_lrc ON d.id = l_lrc.directory_id");
        query.leftJoin("playlist_file pl_f ON d.id = pl_f.directory_id");
        query.leftJoin("artist_info a_i ON d.id = a_i.directory_id");
        query.where("d_child.id IS NULL");
        query.where("t.directory_id IS NULL");
        query.where("i.directory_id IS NULL");
        query.where("l_lrc.directory_id IS NULL");
        query.where("pl_f.directory_id IS NULL");
        query.where("a_i.directory_id IS NULL");

        return utils::execRangeQuery<DirectoryId>(query, range);
    }

    RangeResults<DirectoryId> Directory::findMismatchedLibrary(Session& session, std::optional<Range> range, const std::filesystem::path& rootPath, MediaLibraryId expectedLibraryId)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<DirectoryId>("SELECT d.id FROM directory d") };
        query.where("d.absolute_path = ? OR d.absolute_path LIKE ?").bind(rootPath).bind(getPathWithTrailingSeparator(rootPath).string() + "%");
        query.where("d.media_library_id <> ? OR d.media_library_id IS NULL").bind(expectedLibraryId);

        return utils::execRangeQuery<DirectoryId>(query, range);
    }

    RangeResults<Directory::pointer> Directory::findRootDirectories(Session& session, std::optional<Range> range)
    {
        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<Directory>>("SELECT d from directory d").where("d.parent_directory_id IS NULL") };
        return utils::execRangeQuery<Directory::pointer>(query, range);
    }

    std::vector<std::tuple<Directory::pointer, std::size_t, ReleaseId>> Directory::findFolderListing(Session& session, std::optional<DirectoryId> parentDirectory, std::optional<MediaLibraryId> mediaLibrary)
    {
        session.checkReadTransaction();

        std::ostringstream queryStr;
        queryStr << "SELECT d, COUNT(DISTINCT t.release_id),"
                    " CASE WHEN COUNT(DISTINCT t.release_id) = 1"
                    "  AND NOT EXISTS (SELECT 1 FROM directory d_c"
                    "   INNER JOIN track t2 ON t2.directory_id = d_c.id"
                    "   WHERE d_c.parent_directory_id = d.id)"
                    "  THEN MIN(t.release_id) ELSE NULL END"
                    " FROM directory d"
                    " LEFT JOIN track t ON t.directory_id = d.id"
                    " WHERE ";

        if (parentDirectory)
            queryStr << "d.parent_directory_id = ?";
        else
            queryStr << "d.parent_directory_id IS NULL";

        if (mediaLibrary)
            queryStr << " AND d.media_library_id = ?";

        queryStr << " GROUP BY d.id"
                    " HAVING COUNT(DISTINCT t.release_id) > 0"
                    "  OR EXISTS (SELECT 1 FROM directory d_child WHERE d_child.parent_directory_id = d.id)"
                    " ORDER BY d.name COLLATE NOCASE";

        auto query{ session.getDboSession()->query<std::tuple<Wt::Dbo::ptr<Directory>, long long, ReleaseId>>(queryStr.str()) };

        if (parentDirectory)
            query.bind(*parentDirectory);
        if (mediaLibrary)
            query.bind(*mediaLibrary);

        std::vector<std::tuple<Directory::pointer, std::size_t, ReleaseId>> result;
        for (const auto& [dir, releaseCount, singleReleaseId] : utils::fetchQueryResults<std::tuple<Wt::Dbo::ptr<Directory>, long long, ReleaseId>>(query))
            result.emplace_back(dir, static_cast<std::size_t>(releaseCount), singleReleaseId);

        return result;
    }

    std::vector<std::tuple<Directory::pointer, std::size_t, ReleaseId>> Directory::findFilteredFolderListing(Session& session, std::optional<DirectoryId> parentDirectory, const Filters& filters)
    {
        session.checkReadTransaction();

        std::ostringstream queryStr;
        queryStr << "WITH RECURSIVE filtered_tracks AS ("
                    " SELECT t.directory_id, t.release_id"
                    " FROM track t";

        if (filters.label.isValid())
            queryStr << " INNER JOIN release_label r_l ON r_l.release_id = t.release_id";

        if (filters.releaseType.isValid())
            queryStr << " INNER JOIN release_release_type r_r_t ON r_r_t.release_id = t.release_id";

        if (filters.clusters.size() == 1)
            queryStr << " INNER JOIN track_cluster t_c ON t_c.track_id = t.id";

        queryStr << " WHERE 1 = 1";

        if (filters.mediaLibrary.isValid())
            queryStr << " AND t.media_library_id = ?";

        if (filters.label.isValid())
            queryStr << " AND r_l.label_id = ?";

        if (filters.releaseType.isValid())
            queryStr << " AND r_r_t.release_type_id = ?";

        if (filters.codec)
            queryStr << " AND t.codec = ?";

        if (filters.clusters.size() == 1)
        {
            queryStr << " AND t_c.cluster_id = ?";
        }
        else if (filters.clusters.size() > 1)
        {
            for (std::size_t i{}; i < filters.clusters.size(); ++i)
                queryStr << " AND EXISTS (SELECT 1 FROM track_cluster t_c" << i << " WHERE t_c" << i << ".track_id = t.id AND t_c" << i << ".cluster_id = ?)";
        }

        queryStr << " GROUP BY t.directory_id, t.release_id"
                    "),"
                    " ancestor_walk(directory_id, release_id) AS ("
                    " SELECT f_t.directory_id, f_t.release_id FROM filtered_tracks f_t"
                    " UNION ALL"
                    " SELECT d.parent_directory_id, a_w.release_id"
                    " FROM ancestor_walk a_w"
                    " INNER JOIN directory d ON d.id = a_w.directory_id"
                    " WHERE d.parent_directory_id IS NOT NULL"
                    "),"
                    " child_releases AS ("
                    " SELECT d.id, a_w.release_id"
                    " FROM ancestor_walk a_w"
                    " INNER JOIN directory d ON d.id = a_w.directory_id";

        if (parentDirectory)
            queryStr << " WHERE d.parent_directory_id = ?";
        else
            queryStr << " WHERE d.parent_directory_id IS NULL";

        if (filters.mediaLibrary.isValid())
            queryStr << " AND d.media_library_id = ?";

        queryStr << " GROUP BY d.id, a_w.release_id"
                    ")"
                    " SELECT d, COUNT(*), CASE WHEN COUNT(*) = 1 THEN MIN(c_r.release_id) ELSE NULL END"
                    " FROM child_releases c_r"
                    " INNER JOIN directory d ON d.id = c_r.id"
                    " GROUP BY d.id ORDER BY d.name COLLATE NOCASE";

        auto query{ session.getDboSession()->query<std::tuple<Wt::Dbo::ptr<Directory>, long long, ReleaseId>>(queryStr.str()) };

        if (filters.mediaLibrary.isValid())
            query.bind(filters.mediaLibrary);

        if (filters.label.isValid())
            query.bind(filters.label);

        if (filters.releaseType.isValid())
            query.bind(filters.releaseType);

        if (filters.codec)
            query.bind(detail::getDbCodec(*filters.codec));

        if (filters.clusters.size() == 1)
        {
            query.bind(filters.clusters.front());
        }
        else if (filters.clusters.size() > 1)
        {
            for (ClusterId clusterId : filters.clusters)
                query.bind(clusterId);
        }

        if (parentDirectory)
            query.bind(*parentDirectory);

        if (filters.mediaLibrary.isValid())
            query.bind(filters.mediaLibrary);

        std::vector<std::tuple<Directory::pointer, std::size_t, ReleaseId>> result;
        for (const auto& [dir, releaseCount, singleReleaseId] : utils::fetchQueryResults<std::tuple<Wt::Dbo::ptr<Directory>, long long, ReleaseId>>(query))
            result.emplace_back(dir, static_cast<std::size_t>(releaseCount), singleReleaseId);

        return result;
    }

    std::vector<std::pair<DirectoryId, std::string>> Directory::findBreadcrumbs(Session& session, DirectoryId directoryId)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<std::tuple<DirectoryId, std::string>>(
            "WITH RECURSIVE ancestors(id, display_name, parent_directory_id, depth) AS ("
            " SELECT d.id, COALESCE(NULLIF(d.name, ''), d.absolute_path, '/'), d.parent_directory_id, 0"
            " FROM directory d WHERE d.id = ?"
            " UNION ALL"
            " SELECT d.id, COALESCE(NULLIF(d.name, ''), d.absolute_path, '/'), d.parent_directory_id, a.depth + 1"
            " FROM directory d INNER JOIN ancestors a ON d.id = a.parent_directory_id"
            ")"
            " SELECT id, display_name FROM ancestors ORDER BY depth DESC") };
        query.bind(directoryId);

        std::vector<std::pair<DirectoryId, std::string>> result;
        for (auto& [id, name] : utils::fetchQueryResults<std::tuple<DirectoryId, std::string>>(query))
            result.emplace_back(id, std::move(name));
        return result;
    }

    void Directory::setAbsolutePath(const std::filesystem::path& p)
    {
        assert(p.is_absolute());

        if (!p.has_filename() && p.has_parent_path())
        {
            _absolutePath = p.parent_path();
            _name = _absolutePath.filename();
        }
        else
        {
            _absolutePath = p;
            _name = p.filename();
        }
    }

    void Directory::setParent(ObjectPtr<Directory> parent)
    {
#ifndef NDEBUG
        if (parent)
        {
            assert(_absolutePath.has_parent_path());
            assert(parent->getAbsolutePath() == _absolutePath.parent_path());
        }
#endif

        _parent = getDboPtr(parent);
    }
} // namespace lms::db
