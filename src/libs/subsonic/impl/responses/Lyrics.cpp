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

#include "responses/Lyrics.hpp"

#include "database/TrackLyrics.hpp"

#include "RequestContext.hpp"

namespace lms::api::subsonic
{
    Response::Node createLyricsNode(RequestContext& context, const db::TrackLyrics::pointer& lyrics)
    {
        Response::Node lyricsNode;

        if (!lyrics->getDisplayArtist().empty())
            lyricsNode.setAttribute("artist", lyrics->getDisplayArtist());

        if (!lyrics->getDisplayTitle().empty())
            lyricsNode.setAttribute("title", lyrics->getDisplayTitle());

        std::string lyricsText;
        auto addLine{ [&](std::string&& line) {
            if (!lyricsText.empty())
                lyricsText += "\n";
            lyricsText += std::move(line);
        } };
        if (!lyrics->isSynchronized())
        {
            std::vector<std::string> lines{ lyrics->getUnsynchronizedLines() };
            for (std::string& line : lines)
                addLine(std::move(line));
        }
        else
        {
            // Reconstruct the lyrics without timestamp if needed
            db::TrackLyrics::SynchronizedLines lines{ lyrics->getSynchronizedLines() };
            for (auto& [timestamp, line] : lines)
                addLine(std::move(line));
        }

        switch (context.responseFormat)
        {
        case ResponseFormat::json:
            lyricsNode.setAttribute("value", lyricsText);
            break;
        case ResponseFormat::xml:
            lyricsNode.setValue(lyricsText);
            break;
        }

        return lyricsNode;
    }

    Response::Node createStructuredLyricsNode(RequestContext& context, const db::ObjectPtr<db::TrackLyrics>& lyrics)
    {
        Response::Node lyricsNode;

        if (!lyrics->getDisplayArtist().empty())
            lyricsNode.setAttribute("artist", lyrics->getDisplayArtist());

        if (!lyrics->getDisplayTitle().empty())
            lyricsNode.setAttribute("title", lyrics->getDisplayTitle());

        lyricsNode.setAttribute("lang", lyrics->getLanguage());
        lyricsNode.setAttribute("synced", lyrics->isSynchronized());
        if (lyrics->getOffset() != std::chrono::milliseconds{})
            lyricsNode.setAttribute("offset", lyrics->getOffset().count());

        lyricsNode.createEmptyArrayChild("lines");
        auto addLine{ [&](std::string&& line, std::optional<std::chrono::milliseconds> timestamp = std::nullopt) {
            Response::Node lineNode;
            if (timestamp)
                lineNode.setAttribute("start", std::chrono::duration_cast<std::chrono::milliseconds>(*timestamp).count());

            switch (context.responseFormat)
            {
            case ResponseFormat::json:
                lineNode.setAttribute("value", std::move(line));
                break;
            case ResponseFormat::xml:
                lineNode.setValue(std::move(line));
                break;
            }
            lyricsNode.addArrayChild("lines", std::move(lineNode));
        } };

        if (!lyrics->isSynchronized())
        {
            std::vector<std::string> lines{ lyrics->getUnsynchronizedLines() };

            for (std::string& line : lines)
                addLine(std::move(line));
        }
        else
        {
            db::TrackLyrics::SynchronizedLines lines{ lyrics->getSynchronizedLines() };
            for (auto& [timestamp, line] : lines)
                addLine(std::move(line), timestamp);
        }

        return lyricsNode;
    }

} // namespace lms::api::subsonic