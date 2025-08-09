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

#include "database/objects/Image.hpp"

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Directory.hpp"

#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/PathTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::Image)

namespace lms::db
{
    namespace
    {
        Wt::Dbo::Query<Wt::Dbo::ptr<Image>> createQuery(Session& session, const Image::FindParameters& params)
        {
            auto query{ session.getDboSession()->query<Wt::Dbo::ptr<Image>>("SELECT i FROM image i") };

            if (params.directory.isValid())
                query.where("i.directory_id = ?").bind(params.directory);
            if (!params.fileStem.empty())
            {
                // if contains a wildcard, replace by % for LIKE
                if (params.processWildcardsInFileStem.value() && params.fileStem.find('*') != std::string::npos)
                {
                    std::string fileStem{ params.fileStem };
                    utils::escapeForLikeKeyword(fileStem);
                    std::replace(std::begin(fileStem), std::end(fileStem), '*', '%');
                    query.where("i.stem LIKE ? COLLATE NOCASE ESCAPE '" ESCAPE_CHAR_STR "'").bind(fileStem);
                }
                else
                {
                    query.where("i.stem = ? COLLATE NOCASE").bind(params.fileStem);
                }
            }

            return query;
        }
    } // namespace

    Image::Image(const std::filesystem::path& p)
    {
        setAbsoluteFilePath(p);
    }

    Image::pointer Image::create(Session& session, const std::filesystem::path& p)
    {
        return session.getDboSession()->add(std::unique_ptr<Image>{ new Image{ p } });
    }

    std::size_t Image::getCount(Session& session)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM image"));
    }

    Image::pointer Image::find(Session& session, ImageId id)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<Image>>("SELECT i from image i").where("i.id = ?").bind(id));
    }

    Image::pointer Image::find(Session& session, const std::filesystem::path& file)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<Image>>("SELECT i from image i").where("i.absolute_file_path = ?").bind(file));
    }

    void Image::find(Session& session, ImageId& lastRetrievedId, std::size_t count, const std::function<void(const Image::pointer&)>& func)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<Image>>("SELECT i from image i").orderBy("i.id").where("i.id > ?").bind(lastRetrievedId).limit(static_cast<int>(count)) };

        utils::forEachQueryResult(query, [&](const Image::pointer& image) {
            func(image);
            lastRetrievedId = image->getId();
        });
    }

    RangeResults<Image::pointer> Image::find(Session& session, const FindParameters& params)
    {
        session.checkReadTransaction();

        auto query{ createQuery(session, params) };
        return utils::execRangeQuery<Image::pointer>(query, params.range);
    }

    void Image::find(Session& session, const FindParameters& params, const std::function<void(const Image::pointer&)>& func)
    {
        auto query{ createQuery(session, params) };
        utils::forEachQueryResult(query, [&](const Image::pointer& image) {
            func(image);
        });
    }

    void Image::findAbsoluteFilePath(Session& session, ImageId& lastRetrievedId, std::size_t count, const std::function<void(ImageId imageId, const std::filesystem::path& absoluteFilePath)>& func)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<std::tuple<ImageId, std::filesystem::path>>("SELECT i.id,i.absolute_file_path from image i").orderBy("i.id").where("i.id > ?").bind(lastRetrievedId).limit(static_cast<int>(count)) };

        utils::forEachQueryResult(query, [&](const auto& res) {
            func(std::get<0>(res), std::get<1>(res));
            lastRetrievedId = std::get<0>(res);
        });
    }

    void Image::setAbsoluteFilePath(const std::filesystem::path& p)
    {
        assert(p.is_absolute());
        _fileAbsolutePath = p;
        _fileStem = p.stem().string();
    }

} // namespace lms::db
