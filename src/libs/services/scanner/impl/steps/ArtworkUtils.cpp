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

#include "ArtworkUtils.hpp"

#include "database/Artwork.hpp"
#include "database/Image.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/TrackEmbeddedImage.hpp"

namespace lms::scanner::utils
{
    namespace
    {
        std::filesystem::path toPath(db::Session& session, const db::TrackEmbeddedImageId trackEmbeddedImageId)
        {
            session.checkReadTransaction();

            std::filesystem::path res;

            db::Track::FindParameters params;
            params.setEmbeddedImage(trackEmbeddedImageId);
            params.setRange(db::Range{ .offset = 0, .size = 1 });
            db::Track::find(session, params, [&](const db::Track::pointer& track) {
                res = track->getAbsoluteFilePath();
            });

            return res;
        }

        std::filesystem::path toPath(db::Session& session, db::ImageId imageId)
        {
            session.checkReadTransaction();

            db::Image::pointer image{ db::Image::find(session, imageId) };
            return image ? image->getAbsoluteFilePath() : std::filesystem::path{};
        }

    } // namespace

    db::ObjectPtr<db::Artwork> getOrCreateArtworkFromTrackEmbeddedImage(db::Session& session, db::TrackEmbeddedImageId trackEmbeddedImageId)
    {
        assert(trackEmbeddedImageId.isValid());
        session.checkWriteTransaction();

        db::ObjectPtr<db::Artwork> artwork{ db::Artwork::find(session, trackEmbeddedImageId) };
        if (!artwork)
        {
            db::TrackEmbeddedImage::pointer trackEmbeddedImage{ db::TrackEmbeddedImage::find(session, trackEmbeddedImageId) };
            assert(trackEmbeddedImage);
            artwork = session.create<db::Artwork>(trackEmbeddedImage);
        }
        return artwork;
    }

    db::ObjectPtr<db::Artwork> getOrCreateArtworkFromImage(db::Session& session, db::ImageId imageId)
    {
        assert(imageId.isValid());
        session.checkWriteTransaction();

        db::ObjectPtr<db::Artwork> artwork{ db::Artwork::find(session, imageId) };
        if (!artwork)
        {
            db::Image::pointer image{ db::Image::find(session, imageId) };
            assert(image);
            artwork = session.create<db::Artwork>(image);
        }
        return artwork;
    }

    std::filesystem::path toPath(db::Session& session, db::ArtworkId artworkId)
    {
        session.checkReadTransaction();

        db::Artwork::pointer artwork{ db::Artwork::find(session, artworkId) };
        if (!artwork)
            return std::filesystem::path{};

        if (artwork->getTrackEmbeddedImageId().isValid())
            return toPath(session, artwork->getTrackEmbeddedImageId());
        if (artwork->getImageId().isValid())
            return toPath(session, artwork->getImageId());

        return std::filesystem::path{};
    }

} // namespace lms::scanner::utils