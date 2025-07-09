/*
 * Copyright (C) 2020 Emeric Poupon
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

#pragma once

#include <Wt/WResource.h>
#include <memory>

#include "core/IZipper.hpp"
#include "database/objects/ArtistId.hpp"
#include "database/objects/ReleaseId.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/TrackListId.hpp"

namespace lms::ui
{
    class DownloadResource : public Wt::WResource
    {
    public:
        static constexpr std::size_t bufferSize{ 32768 };

        ~DownloadResource() override;

    private:
        void handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response) override;
        virtual std::unique_ptr<zip::IZipper> createZipper() = 0;
    };

    class DownloadArtistResource : public DownloadResource
    {
    public:
        DownloadArtistResource(db::ArtistId artistId);

    private:
        std::unique_ptr<zip::IZipper> createZipper() override;
        db::ArtistId _artistId;
    };

    class DownloadReleaseResource : public DownloadResource
    {
    public:
        DownloadReleaseResource(db::ReleaseId releaseId);

    private:
        std::unique_ptr<zip::IZipper> createZipper() override;
        db::ReleaseId _releaseId;
    };

    class DownloadTrackResource : public DownloadResource
    {
    public:
        DownloadTrackResource(db::TrackId trackId);

    private:
        std::unique_ptr<zip::IZipper> createZipper() override;
        db::TrackId _trackId;
    };

    class DownloadTrackListResource : public DownloadResource
    {
    public:
        DownloadTrackListResource(db::TrackListId trackListId);

    private:
        std::unique_ptr<zip::IZipper> createZipper() override;
        db::TrackListId _trackListId;
    };
} // namespace lms::ui
