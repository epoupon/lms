/*
 * Copyright (C) 2023 Emeric Poupon
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

#include "Bookmarks.hpp"

#include <array>

#include "database/ISession.hpp"
#include "database/objects/PlayQueue.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackBookmark.hpp"
#include "database/objects/TrackList.hpp"
#include "database/objects/User.hpp"

#include "ParameterParsing.hpp"
#include "SubsonicId.hpp"
#include "responses/Bookmark.hpp"
#include "responses/Song.hpp"

namespace lms::api::subsonic
{
    Response handleGetBookmarks(RequestContext& context)
    {
        auto transaction{ context.getDbSession().createReadTransaction() };

        Response response{ Response::createOkResponse() };
        Response::Node& bookmarksNode{ response.createNode("bookmarks") };

        db::TrackBookmark::find(context.getDbSession(), context.getUser()->getId(), [&](const db::TrackBookmark& bookmark) {
            Response::Node bookmarkNode{ createBookmarkNode(context.getDbSession(), bookmark) };
            if (const std::optional<db::Track> track{ db::Track::find(context.getDbSession(), bookmark.getTrackId()) })
                bookmarkNode.addChild("entry", createSongNode(context, *track));
            bookmarksNode.addArrayChild("bookmark", std::move(bookmarkNode));
        });

        return response;
    }

    Response handleCreateBookmark(RequestContext& context)
    {
        // Mandatory params
        db::TrackId trackId{ getMandatoryParameterAs<db::TrackId>(context.getParameters(), "id") };
        unsigned long position{ getMandatoryParameterAs<unsigned long>(context.getParameters(), "position") };
        const std::optional<std::string> comment{ getParameterAs<std::string>(context.getParameters(), "comment") };

        auto transaction{ context.getDbSession().createWriteTransaction() };

        const std::optional<db::Track> track{ db::Track::find(context.getDbSession(), trackId) };
        if (!track)
            throw RequestedDataNotFoundError{};

        // Replace any existing bookmark
        std::optional<db::TrackBookmark> bookmark{ db::TrackBookmark::find(context.getDbSession(), context.getUser()->getId(), trackId) };
        if (!bookmark)
            bookmark = db::TrackBookmark::create(context.getDbSession(), context.getUser()->getId(), trackId);

        bookmark->setOffset(std::chrono::milliseconds{ position });
        if (comment)
            bookmark->setComment(*comment);
        db::TrackBookmark::update(context.getDbSession(), *bookmark);

        return Response::createOkResponse();
    }

    Response handleDeleteBookmark(RequestContext& context)
    {
        // Mandatory params
        db::TrackId trackId{ getMandatoryParameterAs<db::TrackId>(context.getParameters(), "id") };

        auto transaction{ context.getDbSession().createWriteTransaction() };

        const std::optional<db::TrackBookmark> bookmark{ db::TrackBookmark::find(context.getDbSession(), context.getUser()->getId(), trackId) };
        if (!bookmark)
            throw RequestedDataNotFoundError{};

        db::TrackBookmark::destroy(context.getDbSession(), std::array{ bookmark->getId() });

        return Response::createOkResponse();
    }

    static db::PlayQueue getOrCreatePlayQueue(RequestContext& context)
    {
        {
            auto transaction{ context.getDbSession().createReadTransaction() };

            if (std::optional<db::PlayQueue> playQueue{ db::PlayQueue::find(context.getDbSession(), context.getUser()->getId(), db::PlayQueueType::Subsonic) })
                return *playQueue;
        }

        auto transaction{ context.getDbSession().createWriteTransaction() };

        if (std::optional<db::PlayQueue> playQueue{ db::PlayQueue::find(context.getDbSession(), context.getUser()->getId(), db::PlayQueueType::Subsonic) })
            return *playQueue;

        db::PlayQueue playQueue{ db::PlayQueue::create(context.getDbSession(), context.getUser()->getId(), db::PlayQueueType::Subsonic) };
        playQueue.setLastModifiedDateTime(Wt::WDateTime::currentDateTime());
        db::PlayQueue::update(context.getDbSession(), playQueue);

        return playQueue;
    }

    static Response handleGetPlayQueueCommon(RequestContext& context, bool byIndex)
    {
        Response response{ Response::createOkResponse() };

        const db::PlayQueue playQueue{ getOrCreatePlayQueue(context) };

        auto transaction{ context.getDbSession().createReadTransaction() };

        Response::Node& playQueueNode{ response.createNode(byIndex ? core::LiteralString{ "playQueueByIndex" } : core::LiteralString{ "playQueue" }) };

        // mandatory fields
        playQueueNode.setAttribute("username", context.getUser()->getLoginName());
        playQueueNode.setAttribute("changed", core::stringUtils::toISO8601String(playQueue.getLastModifiedDateTime()));
        playQueueNode.setAttribute("changedBy", ""); // we don't store the client name (could be several same clients on several devices...)

        // optional fields
        if (!db::TrackList::isEmpty(context.getDbSession(), playQueue.getTrackListId()))
        {
            if (byIndex)
            {
                playQueueNode.setAttribute("index", playQueue.getCurrentIndex());
            }
            else
            {
                const auto currentTrackIds{ db::Track::findIds(context.getDbSession(), db::Track::FindParameters{}.setTrackList(playQueue.getTrackListId()).setRange(db::Range{ playQueue.getCurrentIndex(), 1 })) };
                if (!currentTrackIds.empty())
                    playQueueNode.setAttribute("current", idToString(currentTrackIds.front()));
            }

            playQueueNode.setAttribute("position", std::chrono::duration_cast<std::chrono::milliseconds>(playQueue.getCurrentPositionInTrack()).count());

            db::Track::find(context.getDbSession(), db::Track::FindParameters{}.setTrackList(playQueue.getTrackListId()), [&](const db::Track& track) {
                playQueueNode.addArrayChild("entry", createSongNode(context, track));
            });
        }

        return response;
    }

    Response handleGetPlayQueue(RequestContext& context)
    {
        return handleGetPlayQueueCommon(context, false);
    }

    Response handleGetPlayQueueByIndex(RequestContext& context)
    {
        return handleGetPlayQueueCommon(context, true);
    }

    static Response handleSavePlayQueueCommon(RequestContext& context, bool byIndex)
    {
        std::vector<db::TrackId> trackIds{ getMultiParametersAs<db::TrackId>(context.getParameters(), "id") };
        const std::optional<db::TrackId> currentTrackId{ byIndex ? std::nullopt : getParameterAs<db::TrackId>(context.getParameters(), "current") };
        const std::optional<std::size_t> currentIndex{ byIndex ? getParameterAs<std::size_t>(context.getParameters(), "currentIndex") : std::nullopt };
        const std::chrono::milliseconds currentPositionInTrack{ getParameterAs<std::size_t>(context.getParameters(), "position").value_or(0) };

        db::PlayQueue playQueue{ getOrCreatePlayQueue(context) };

        {
            auto transaction{ context.getDbSession().createWriteTransaction() };

            // no id means we clear the play queue (see https://github.com/opensubsonic/open-subsonic-api/pull/106)
            db::TrackList::clear(context.getDbSession(), playQueue.getTrackListId());

            playQueue.setCurrentIndex(0);
            bool currentFound{};
            bool currentSkipped{};
            std::size_t currentPlayQueueIndex{};
            std::size_t currentTrackIndex{};
            for (db::TrackId trackId : trackIds)
            {
                if (const std::optional<db::Track> track{ db::Track::find(context.getDbSession(), trackId) })
                {
                    db::TrackListEntry::create(context.getDbSession(), trackId, playQueue.getTrackListId());

                    // "current" is a track id: when the track is queued several times, take its first occurrence
                    // "currentIndex" may point to an unknown track: resume on the next known one
                    if (!currentFound
                        && ((currentTrackId && *currentTrackId == track->getId())
                            || (currentIndex && currentTrackIndex >= *currentIndex)))
                    {
                        playQueue.setCurrentIndex(currentPlayQueueIndex);
                        currentFound = true;
                        currentSkipped = currentIndex && currentTrackIndex != *currentIndex;
                    }

                    currentPlayQueueIndex++;
                }

                currentTrackIndex++;
            }

            // The position only applies to the requested current track
            playQueue.setCurrentPositionInTrack(currentFound && !currentSkipped ? currentPositionInTrack : std::chrono::milliseconds{ 0 });
            playQueue.setLastModifiedDateTime(Wt::WDateTime::currentDateTime());
            db::PlayQueue::update(context.getDbSession(), playQueue);
        }

        return Response::createOkResponse();
    }

    Response handleSavePlayQueue(RequestContext& context)
    {
        return handleSavePlayQueueCommon(context, false);
    }

    Response handleSavePlayQueueByIndex(RequestContext& context)
    {
        return handleSavePlayQueueCommon(context, true);
    }
} // namespace lms::api::subsonic