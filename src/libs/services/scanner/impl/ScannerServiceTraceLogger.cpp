/*
 * Copyright (C) 2013 Emeric Poupon
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

#include "ScannerService.hpp"

#include "database/objects/Artist.hpp"
#include "database/objects/ArtistInfo.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Image.hpp"
#include "database/objects/Listen.hpp"
#include "database/objects/StarredArtist.hpp"
#include "database/objects/StarredRelease.hpp"
#include "database/objects/StarredTrack.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/TrackBookmark.hpp"
#include "database/objects/TrackEmbeddedImage.hpp"
#include "database/objects/TrackEmbeddedImageLink.hpp"

namespace lms::scanner
{
    void ScannerService::refreshTracingLoggerStats()
    {
        auto* traceLogger{ core::Service<core::tracing::ITraceLogger>::get() };
        if (!traceLogger)
            return;

        auto& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        traceLogger->setMetadata("db_artist_count", std::to_string(db::Artist::getCount(session)));
        traceLogger->setMetadata("db_artist_info_count", std::to_string(db::ArtistInfo::getCount(session)));
        traceLogger->setMetadata("db_cluster_count", std::to_string(db::Cluster::getCount(session)));
        traceLogger->setMetadata("db_cluster_type_count", std::to_string(db::ClusterType::getCount(session)));
        traceLogger->setMetadata("db_image_count", std::to_string(db::Image::getCount(session)));
        traceLogger->setMetadata("db_listen_count", std::to_string(db::Listen::getCount(session)));
        traceLogger->setMetadata("db_release_count", std::to_string(db::Release::getCount(session)));
        traceLogger->setMetadata("db_starred_artist_count", std::to_string(db::StarredArtist::getCount(session)));
        traceLogger->setMetadata("db_starred_release_count", std::to_string(db::StarredRelease::getCount(session)));
        traceLogger->setMetadata("db_starred_track_count", std::to_string(db::StarredTrack::getCount(session)));
        traceLogger->setMetadata("db_track_bookmark_count", std::to_string(db::TrackBookmark::getCount(session)));
        traceLogger->setMetadata("db_track_count", std::to_string(db::Track::getCount(session)));
        traceLogger->setMetadata("db_track_artist_link_count", std::to_string(db::TrackArtistLink::getCount(session)));
        traceLogger->setMetadata("db_track_embedded_image_count", std::to_string(db::TrackEmbeddedImage::getCount(session)));
        traceLogger->setMetadata("db_track_embedded_image_link_count", std::to_string(db::TrackEmbeddedImageLink::getCount(session)));
    }
} // namespace lms::scanner