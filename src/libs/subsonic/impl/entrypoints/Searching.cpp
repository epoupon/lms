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

#include "Searching.hpp"

#include <chrono>
#include <mutex>
#include <map>

#include "database/Artist.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/User.hpp"
#include "responses/Album.hpp"
#include "responses/Artist.hpp"
#include "responses/Song.hpp"
#include "ParameterParsing.hpp"
#include "SubsonicId.hpp"

namespace lms::api::subsonic
{
    using namespace db;

    namespace
    {
        // Search endpoints can be used to scan/sync the database
        // This class is used to keep track of the current scans, in order to retrieve the last objectId
        // to speed up the query of the following range (avoid the 'offset' cost)
        template <typename ObjectId>
        class ScanTracker
        {
        public:
            struct ScanInfo
            {
                std::string clientAddress;
                std::string clientName;
                std::string userName;
                MediaLibraryId library;
                std::size_t offset{};
                auto operator<=>(const ScanInfo&) const = default;
            };

            ObjectId extractLastRetrievedObjectId(const ScanInfo& info);
            void setObjectId(const ScanInfo& info, ObjectId lastRetrievedId);
            void cleanOutdatedScanEntries();

        private:
            using ClockType = std::chrono::steady_clock;

            struct Entry
            {
                ClockType::time_point timePoint;
                ObjectId objectId;
            };

            static constexpr ClockType::duration maxEntryDuration{ std::chrono::seconds{30} };

            std::mutex _mutex;
            std::map<ScanInfo, Entry> _ongoingScans;
        };

        template<typename ObjectId>
        ObjectId ScanTracker<ObjectId>::extractLastRetrievedObjectId(const ScanInfo& scanInfo)
        {
            ObjectId res;

            const std::scoped_lock lock{ _mutex };
            auto it{ _ongoingScans.find(scanInfo) };
            if (it != _ongoingScans.end())
            {
                res = it->second.objectId;
                _ongoingScans.erase(it);
            }

            return res;
        }

        template<typename ObjectId>
        void ScanTracker<ObjectId>::setObjectId(const ScanInfo& scanInfo, ObjectId lastRetrievedId)
        {
            const std::scoped_lock lock{ _mutex };
            _ongoingScans[scanInfo] = { ClockType::now(), lastRetrievedId };
        }

        template<typename ObjectId>
        void ScanTracker<ObjectId>::cleanOutdatedScanEntries()
        {
            const ClockType::time_point now{ ClockType::now() };

            const std::scoped_lock lock{ _mutex };
            std::erase_if(_ongoingScans, [&](const auto& entry) { return now > entry.second.timePoint + maxEntryDuration; });
        }
    }

    namespace
    {
        Response handleSearchRequestCommon(RequestContext& context, bool id3)
        {
            // Mandatory params
            const std::string queryString{ getMandatoryParameterAs<std::string>(context.parameters, "query") };
            std::string_view query{ queryString };

            // Optional params
            const std::size_t artistCount{ getParameterAs<std::size_t>(context.parameters, "artistCount").value_or(20) };
            const std::size_t artistOffset{ getParameterAs<std::size_t>(context.parameters, "artistOffset").value_or(0) };
            const std::size_t albumCount{ getParameterAs<std::size_t>(context.parameters, "albumCount").value_or(20) };
            const std::size_t albumOffset{ getParameterAs<std::size_t>(context.parameters, "albumOffset").value_or(0) };
            const std::size_t songCount{ getParameterAs<std::size_t>(context.parameters, "songCount").value_or(20) };
            const std::size_t songOffset{ getParameterAs<std::size_t>(context.parameters, "songOffset").value_or(0) };
            const MediaLibraryId mediaLibrary{ getParameterAs<MediaLibraryId>(context.parameters, "musicFolderId").value_or(MediaLibraryId{}) };

            if (artistCount > defaultMaxCountSize)
                throw ParameterValueTooHighGenericError{ "artistCount", defaultMaxCountSize };
            if (albumCount > defaultMaxCountSize)
                throw ParameterValueTooHighGenericError{ "albumCount", defaultMaxCountSize };
            if (songCount > defaultMaxCountSize)
                throw ParameterValueTooHighGenericError{ "songCount", defaultMaxCountSize };

            // Symfonium adds extra ""
            if (context.clientInfo.name == "Symfonium")
                query = core::stringUtils::stringTrim(query, "\"");

            std::vector<std::string_view> keywords;
            if (!query.empty())
                keywords = core::stringUtils::splitString(query, ' ');

            Response response{ Response::createOkResponse(context.serverProtocolVersion) };
            Response::Node& searchResult2Node{ response.createNode(id3 ? "searchResult3" : "searchResult2") };

            auto transaction{ context.dbSession.createReadTransaction() };

            User::pointer user{ User::find(context.dbSession, context.userId) };
            if (!user)
                throw UserNotAuthorizedError{};

            if (artistCount > 0)
            {
                Artist::FindParameters params;
                params.setKeywords(keywords);
                params.setRange(Range{ artistOffset, artistCount });
                params.setMediaLibrary(mediaLibrary);

                Artist::find(context.dbSession, params, [&](const Artist::pointer& artist)
                    {
                        searchResult2Node.addArrayChild("artist", createArtistNode(context, artist, user, id3));
                    });
            }

            if (albumCount > 0)
            {
                Release::FindParameters params;
                params.setKeywords(keywords);
                params.setRange(Range{ albumOffset, albumCount });
                params.setMediaLibrary(mediaLibrary);

                Release::find(context.dbSession, params, [&](const Release::pointer& release)
                    {
                        searchResult2Node.addArrayChild("album", createAlbumNode(context, release, user, id3));
                    });
            }

            if (songCount > 0)
            {
                static ScanTracker<TrackId> currentScansInProgress;
                currentScansInProgress.cleanOutdatedScanEntries();

                TrackId lastRetrievedId;

                auto findTracks{ [&]
                {
                    Track::FindParameters params;
                    params.setKeywords(keywords);
                    params.setRange(Range{ songOffset, songCount });
                    params.setMediaLibrary(mediaLibrary);

                    Track::find(context.dbSession, params, [&](const Track::pointer& track)
                        {
                            searchResult2Node.addArrayChild("song", createSongNode(context, track, user));
                            lastRetrievedId = track->getId();
                        });
                } };

                if (!keywords.empty())
                {
                    findTracks();
                }
                else
                {
                    ScanTracker<TrackId>::ScanInfo scanInfo
                    {
                        .clientAddress = context.clientInfo.ipAddress,
                        .clientName = context.clientInfo.name,
                        .userName = context.clientInfo.user,
                        .library = mediaLibrary,
                        .offset = songOffset
                    };

                    if (TrackId cachedLastRetrievedId{ currentScansInProgress.extractLastRetrievedObjectId(scanInfo) }; cachedLastRetrievedId.isValid())
                    {
                        Track::find(context.dbSession, cachedLastRetrievedId, songCount, [&](const Track::pointer& track)
                            {
                                searchResult2Node.addArrayChild("song", createSongNode(context, track, user));
                            }, mediaLibrary);
                        lastRetrievedId = cachedLastRetrievedId;
                    }
                    else
                    {
                        findTracks();
                    }

                    if (lastRetrievedId.isValid())
                    {
                        scanInfo.offset = songOffset + songCount;
                        currentScansInProgress.setObjectId(scanInfo, lastRetrievedId);
                    }
                }
            }

            return response;
        }
    }

    Response handleSearch2Request(RequestContext& context)
    {
        return handleSearchRequestCommon(context, false /* no id3 */);
    }

    Response handleSearch3Request(RequestContext& context)
    {
        return handleSearchRequestCommon(context, true /* id3 */);
    }
}