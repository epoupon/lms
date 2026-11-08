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

#include "database/objects/TrackEmbeddedImage.hpp"

#include <Wt/Dbo/Impl.h>

#include "database/Types.hpp"

#include "ObjectPtrImpl.hpp"
#include "Session.hpp"
#include "TrackDbo.hpp"
#include "TrackEmbeddedImageDbo.hpp"
#include "TrackEmbeddedImageLinkDbo.hpp"
#include "Utils.hpp"
#include "detail/Types.hpp"
#include "objects/detail/Types.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/ImageHashTypeTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::TrackEmbeddedImage)

namespace lms::db
{
    namespace
    {
        Wt::Dbo::Query<Wt::Dbo::ptr<dbo::TrackEmbeddedImage>> createQuery(ISession& session, const TrackEmbeddedImage::FindParameters& params)
        {
            checkReadTransaction(session);

            auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::TrackEmbeddedImage>>("SELECT t_e_i FROM track_embedded_image t_e_i") };

            if (params.medium.isValid()
                || params.track.isValid()
                || params.release.isValid()
                || params.trackList.isValid()
                || params.imageType.has_value()
                || params.sortMethod == TrackEmbeddedImageSortMethod::DiscNumberThenTrackNumberThenSizeDesc
                || params.sortMethod == TrackEmbeddedImageSortMethod::TrackNumberThenSizeDesc)
            {
                query.join("track_embedded_image_link t_e_i_l ON t_e_i_l.track_embedded_image_id = t_e_i.id");

                if (params.track.isValid())
                    query.where("t_e_i_l.track_id = ?").bind(params.track);

                if (params.release.isValid()
                    || params.medium.isValid()
                    || params.sortMethod == TrackEmbeddedImageSortMethod::DiscNumberThenTrackNumberThenSizeDesc
                    || params.sortMethod == TrackEmbeddedImageSortMethod::TrackNumberThenSizeDesc)
                {
                    assert(!(params.release.isValid() && params.medium.isValid()));

                    assert((params.sortMethod != TrackEmbeddedImageSortMethod::DiscNumberThenTrackNumberThenSizeDesc || params.release.isValid()));
                    assert((params.sortMethod != TrackEmbeddedImageSortMethod::TrackNumberThenSizeDesc || params.medium.isValid()));

                    query.join("track t ON t_e_i_l.track_id = t.id");
                    if (params.release.isValid())
                        query.where("t.release_id = ?").bind(params.release);
                    if (params.medium.isValid())
                        query.where("t.medium_id = ?").bind(params.medium);

                    if (params.sortMethod == TrackEmbeddedImageSortMethod::DiscNumberThenTrackNumberThenSizeDesc)
                        query.join("medium m ON m.id = t.medium_id");
                }

                if (params.trackList.isValid())
                {
                    query.join("tracklist_entry t_l_e ON t_l_e.track_id = t_e_i_l.track_id");
                    query.where("t_l_e.tracklist_id = ?").bind(params.trackList);
                }

                if (params.imageType.has_value())
                    query.where("t_e_i_l.type = ?").bind(detail::getDbImageType(params.imageType.value()));
            }

            switch (params.sortMethod)
            {
            case TrackEmbeddedImageSortMethod::None:
                break;
            case TrackEmbeddedImageSortMethod::SizeDesc:
                query.orderBy("t_e_i.size DESC");
                break;
            case TrackEmbeddedImageSortMethod::DiscNumberThenTrackNumberThenSizeDesc:
                query.orderBy("m.position, t.track_number, t_e_i.size DESC");
                break;
            case TrackEmbeddedImageSortMethod::TrackNumberThenSizeDesc:
                query.orderBy("t.track_number, t_e_i.size DESC");
                break;
            case TrackEmbeddedImageSortMethod::TrackListIndexAscThenSizeDesc:
                assert(params.trackList.isValid());
                query.orderBy("t_l_e.id, t_e_i.size DESC");
                break;
            }

            return query;
        }
    } // namespace

    TrackEmbeddedImage::pointer TrackEmbeddedImage::create(ISession& session)
    {
        return utils::addObject(session, std::unique_ptr<dbo::TrackEmbeddedImage>{ new dbo::TrackEmbeddedImage{} });
    }

    void TrackEmbeddedImage::destroy(ISession& session, std::span<const TrackEmbeddedImageId> ids)
    {
        utils::destroyObjects<dbo::TrackEmbeddedImage>(session, ids);
    }

    std::size_t TrackEmbeddedImage::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM track_embedded_image"));
    }

    TrackEmbeddedImage::pointer TrackEmbeddedImage::find(ISession& session, TrackEmbeddedImageId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::TrackEmbeddedImage>().where("id = ?").bind(id));
    }

    void TrackEmbeddedImage::find(ISession& session, const FindParameters& params, const std::function<void(const pointer&)>& func)
    {
        checkReadTransaction(session);

        auto query{ createQuery(session, params) };
        utils::forEachQueryRangeObject(query, params.range, func);
    }

    TrackEmbeddedImage::pointer TrackEmbeddedImage::find(ISession& session, std::size_t size, ImageHashType hash)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->find<dbo::TrackEmbeddedImage>().where("size = ?").bind(static_cast<int>(size)).where("hash = ?").bind(hash) };
        return utils::fetchQuerySingleObject(query);
    }

    std::vector<TrackEmbeddedImageId> TrackEmbeddedImage::findOrphanIds(ISession& session, std::optional<Range> range)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<TrackEmbeddedImageId>("SELECT t_e_i.id FROM track_embedded_image t_e_i LEFT JOIN track_embedded_image_link t_e_i_l ON t_e_i.id = t_e_i_l.track_embedded_image_id WHERE t_e_i_l.track_embedded_image_id IS NULL") };
        return utils::execRangeQuery<TrackEmbeddedImageId>(query, range);
    }

    std::optional<core::media::ImageFormat> TrackEmbeddedImage::getFormat() const
    {
        return detail::getMediaImageFormat(_format);
    }

    void TrackEmbeddedImage::setFormat(core::media::ImageFormat format)
    {
        _format = detail::getDbImageFormat(format);
    }

} // namespace lms::db
