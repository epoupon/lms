/*
 * Copyright (C) 2026 Emeric Poupon
 *
 * This file is part of LMS.
 *
 * LMS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
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

#include "PlaylistExportResource.hpp"

#include <chrono>
#include <Wt/Http/Response.h>
#include <Wt/WDateTime.h>

#include "database/Session.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackList.hpp"

#include "LmsApplication.hpp"
#include "core/String.hpp"

namespace lms::ui
{
    PlaylistExportResource::PlaylistExportResource(db::TrackListId trackListId)
        : _trackListId{ trackListId }
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };
        const db::TrackList::pointer trackList{ db::TrackList::find(LmsApp->getDbSession(), trackListId) };
        if (trackList)
            suggestFileName(core::stringUtils::replaceInString(std::string{ trackList->getName() }, "/", "_") + ".m3u");
    }

    void PlaylistExportResource::handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response)
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        const db::TrackList::pointer trackList{ db::TrackList::find(LmsApp->getDbSession(), _trackListId) };
        if (!trackList)
        {
            response.setStatus(404);
            return;
        }

        response.setMimeType("audio/x-mpegurl");
        
        response.out() << "#EXTM3U\n";
        
        auto entries{ trackList->getEntries() };
        for (const auto& entry : entries.results)
        {
            if (auto track{ entry->getTrack() })
            {
                long long durationSeconds = std::chrono::duration_cast<std::chrono::seconds>(track->getDuration()).count();
                std::string artist{ track->getArtistDisplayName() };
                std::string title = track->getName();

                response.out() << "#EXTINF:" << durationSeconds << "," << artist << " - " << title << "\n";
                response.out() << track->getAbsoluteFilePath().string() << "\n";
            }
        }
    }
} // namespace lms::ui
