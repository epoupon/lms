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

#include <Wt/Dbo/Impl.h>

#include "database/Session.hpp"
#include "database/Types.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "database/objects/Release.hpp"

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

    std::vector<Directory::pointer> Directory::find(Session& session, const FindParameters& params)
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

    std::vector<DirectoryId> Directory::findOrphanIds(Session& session, std::optional<Range> range)
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

    std::vector<DirectoryId> Directory::findMismatchedLibrary(Session& session, std::optional<Range> range, const std::filesystem::path& rootPath, MediaLibraryId expectedLibraryId)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<DirectoryId>("SELECT d.id FROM directory d") };
        query.where("d.absolute_path = ? OR d.absolute_path LIKE ?").bind(rootPath).bind(getPathWithTrailingSeparator(rootPath).string() + "%");
        query.where("d.media_library_id <> ? OR d.media_library_id IS NULL").bind(expectedLibraryId);

        return utils::execRangeQuery<DirectoryId>(query, range);
    }

    std::vector<Directory::pointer> Directory::findRootDirectories(Session& session, std::optional<Range> range)
    {
        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<Directory>>("SELECT d from directory d").where("d.parent_directory_id IS NULL") };
        return utils::execRangeQuery<Directory::pointer>(query, range);
    }

    std::vector<Directory::ChildRelease> Directory::findChildReleases(Session& session, DirectoryId parentDirectory)
    {
        session.checkReadTransaction();

        // Resolves the whole listing at once.
        //
        // Read the joins from the track outwards, since tracks are what tie the two together (a
        // directory has no direct link to a release):
        //   track -> directory restricts to tracks whose directory is a child of parentDirectory
        //   track -> release turns the track's release_id into a release row we can return
        //
        // Both are INNER JOINs, so a child directory holding no track, or only tracks belonging to
        // no release, produces no row at all. Such directories are absent from the result rather
        // than present with an empty release.
        //
        // GROUP BY then collapses the many tracks of a directory down to one row per directory.
        //
        // The count is returned rather than acted on here so each caller can apply its own rule:
        // subsonic treats any release as the directory's album, while the folder view only links
        // straight to a release when the directory holds exactly one.
        auto query{ session.getDboSession()->query<std::tuple<DirectoryId, Wt::Dbo::ptr<Release>, int>>(
            "SELECT t.directory_id, r, COUNT(DISTINCT t.release_id)"
            " FROM track t"
            " INNER JOIN directory d ON d.id = t.directory_id"
            " INNER JOIN release r ON r.id = t.release_id") };
        query.where("d.parent_directory_id = ?").bind(parentDirectory);
        query.groupBy("t.directory_id");

        std::vector<ChildRelease> result;
        for (const auto& [directoryId, release, releaseCount] : utils::fetchQueryResults<std::tuple<DirectoryId, Wt::Dbo::ptr<Release>, int>>(query))
            result.emplace_back(ChildRelease{ directoryId, release, static_cast<std::size_t>(releaseCount) });

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
