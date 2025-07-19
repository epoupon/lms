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

#include "database/Session.hpp"
#include "database/Types.hpp"
#include "database/objects/Track.hpp"

#include "database/objects/TrackEmbeddedImageLink.hpp"

#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/ImageHashTypeTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::TrackEmbeddedImage)

namespace lms::db
{
    namespace
    {
        Wt::Dbo::Query<Wt::Dbo::ptr<TrackEmbeddedImage>> createQuery(Session& session, const TrackEmbeddedImage::FindParameters& params)
        {
            session.checkReadTransaction();

            auto query{ session.getDboSession()->query<Wt::Dbo::ptr<TrackEmbeddedImage>>("SELECT t_e_i FROM track_embedded_image t_e_i") };

            if (params.artist.isValid()
                || params.discNumber.has_value()
                || params.track.isValid()
                || params.release.isValid()
                || params.trackList.isValid()
                || params.imageType.has_value()
                || params.sortMethod == TrackEmbeddedImageSortMethod::DiscNumberThenTrackNumberThenSizeDesc
                || params.sortMethod == TrackEmbeddedImageSortMethod::TrackNumberThenSizeDesc)
            {
                query.join("track_embedded_image_link t_e_i_l ON t_e_i_l.track_embedded_image_id = t_e_i.id");

                if (params.artist.isValid())
                {
                    query.join("track_artist_link t_a_l ON t_a_l.track_id = t_e_i_l.track_id");
                    query.where("t_a_l.artist_id = ?").bind(params.artist);

                    if (!params.trackArtistLinkTypes.empty())
                    {
                        std::string clause{ "t_a_l.type IN (" };
                        for (const auto& type : params.trackArtistLinkTypes)
                        {
                            if (clause.back() != '(')
                                clause += ",";
                            clause += "?";
                            query.bind(type);
                        }
                        clause += ")";
                        query.where(clause);
                    }
                }

                if (params.track.isValid())
                    query.where("t_e_i_l.track_id = ?").bind(params.track);

                if (params.release.isValid()
                    || params.discNumber.has_value()
                    || params.sortMethod == TrackEmbeddedImageSortMethod::DiscNumberThenTrackNumberThenSizeDesc
                    || params.sortMethod == TrackEmbeddedImageSortMethod::TrackNumberThenSizeDesc)
                {
                    query.join("track t ON t_e_i_l.track_id = t.id");
                    if (params.release.isValid())
                        query.where("t.release_id = ?").bind(params.release);
                    if (params.discNumber.has_value())
                        query.where("t.disc_number = ?").bind(params.discNumber.value());
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
            case TrackEmbeddedImageSortMethod::DiscNumberThenTrackNumberThenSizeDesc:
                query.orderBy("t.disc_number, t.track_number, t_e_i.size DESC");
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
