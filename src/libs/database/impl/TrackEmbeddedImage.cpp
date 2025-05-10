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

#include "database/Directory.hpp"

#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/TrackEmbeddedImage.hpp"
#include "database/Types.hpp"

#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/ImageHashTypeTraits.hpp"

namespace lms::db
{
    namespace
    {
        Wt::Dbo::Query<Wt::Dbo::ptr<TrackEmbeddedImage>> createQuery(Session& session, const TrackEmbeddedImage::FindParameters& params)
        {
            session.checkReadTransaction();

            auto query{ session.getDboSession()->query<Wt::Dbo::ptr<TrackEmbeddedImage>>("SELECT t_e_i FROM track_embedded_image t_e_i") };

            if (params.track.isValid()
                || params.release.isValid()
                || params.trackList.isValid()
                || params.imageType.has_value()
                || params.sortMethod == TrackEmbeddedImageSortMethod::MediaTypeThenFrontTypeThenSizeDescDesc
                || params.sortMethod == TrackEmbeddedImageSortMethod::FrontTypeThenSizeDesc)
            {
                query.join("track_embedded_image_link t_e_i_l ON t_e_i_l.track_embedded_image_id = t_e_i.id");

                if (params.track.isValid())
                    query.where("t_e_i_l.track_id = ?").bind(params.track);

                if (params.release.isValid())
                {
                    query.join("track t ON t_e_i_l.track_id = t.id");
                    query.where("t.release_id = ?").bind(params.release);
                }

                if (params.trackList.isValid())
                {
                    query.join("tracklist_entry t_l_e ON t_l_e.track_id = t_e_i_l.track_id");
                    query.where("t_l_e.tracklist_id = ?").bind(params.trackList);
                }

                if (params.imageType.has_value())
                    query.where("t_e_i_l.type = ?").bind(params.imageType.value());
            }

            switch (params.sortMethod)
            {
            case TrackEmbeddedImageSortMethod::None:
                break;
            case TrackEmbeddedImageSortMethod::SizeDesc:
                query.orderBy("t_e_i.size DESC");
                break;
            case TrackEmbeddedImageSortMethod::MediaTypeThenFrontTypeThenSizeDescDesc:
                query.orderBy("CASE t_e_i_l.type WHEN ? THEN 1 WHEN ? THEN 2 ELSE 3 END, t_e_i.size DESC").bind(ImageType::Media).bind(ImageType::FrontCover);
                break;
            case TrackEmbeddedImageSortMethod::FrontTypeThenSizeDesc:
                query.orderBy("CASE WHEN t_e_i_l.type = ? THEN 0 ELSE 1 END, t_e_i.size DESC").bind(ImageType::FrontCover);
                break;
            }

            return query;
        }
    } // namespace

    TrackEmbeddedImage::pointer TrackEmbeddedImage::create(Session& session)
    {
        return session.getDboSession()->add(std::unique_ptr<TrackEmbeddedImage>{ new TrackEmbeddedImage{} });
    }

    std::size_t TrackEmbeddedImage::getCount(Session& session)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM track_embedded_image"));
    }

    TrackEmbeddedImage::pointer TrackEmbeddedImage::find(Session& session, TrackEmbeddedImageId id)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->find<TrackEmbeddedImage>().where("id = ?").bind(id));
    }

    void TrackEmbeddedImage::find(Session& session, const FindParameters& params, const std::function<void(const pointer&)>& func)
    {
        session.checkReadTransaction();

        auto query{ createQuery(session, params) };
        utils::forEachQueryRangeResult(query, params.range, func);
    }

    TrackEmbeddedImage::pointer TrackEmbeddedImage::find(Session& session, std::size_t size, ImageHashType hash)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->find<TrackEmbeddedImage>().where("size = ?").bind(static_cast<int>(size)).where("hash = ?").bind(hash) };
        return utils::fetchQuerySingleResult(query);
    }

    RangeResults<TrackEmbeddedImageId> TrackEmbeddedImage::findOrphanIds(Session& session, std::optional<Range> range)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<TrackEmbeddedImageId>("SELECT t_e_i.id FROM track_embedded_image t_e_i LEFT JOIN track_embedded_image_link t_e_i_l ON t_e_i.id = t_e_i_l.track_embedded_image_id WHERE t_e_i_l.track_embedded_image_id IS NULL") };
        return utils::execRangeQuery<TrackEmbeddedImageId>(query, range);
    }

} // namespace lms::db
