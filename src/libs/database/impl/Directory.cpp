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

#include "database/Directory.hpp"

#include "database/Session.hpp"

#include "IdTypeTraits.hpp"
#include "PathTraits.hpp"
#include "Utils.hpp"

namespace lms::db
{
    namespace
    {
        Wt::Dbo::Query<Wt::Dbo::ptr<Directory>> createQuery(Session& session, const Directory::FindParameters& params)
        {
            auto query{ session.getDboSession()->query<Wt::Dbo::ptr<Directory>>("SELECT d FROM directory d") };

            if (params.artist.isValid())
            {
                query.join("track t ON t.directory_id = d.id")
                    .join("artist a ON a.id = t_a_l.artist_id")
                    .join("track_artist_link t_a_l ON t_a_l.track_id = t.id")
                    .where("a.id = ?")
                    .bind(params.artist);

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

                query.groupBy("d.id");
            }

            return query;
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

    void Directory::find(Session& session, const FindParameters& params, const std::function<void(const Directory::pointer&)>& func)
    {
        auto query{ createQuery(session, params) };
        utils::forEachQueryResult(query, [&func](const Directory::pointer& dir) {
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
        query.where("d_child.id IS NULL");
        query.where("t.directory_id IS NULL");
        query.where("i.directory_id IS NULL");

        return utils::execRangeQuery<DirectoryId>(query, range);
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
