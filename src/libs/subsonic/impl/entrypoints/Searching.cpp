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
#include <map>
#include <mutex>

#include "core/Random.hpp"
#include "database/Artist.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/User.hpp"

#include "ParameterParsing.hpp"
#include "SubsonicId.hpp"
#include "responses/Album.hpp"
#include "responses/Artist.hpp"
#include "responses/Song.hpp"

namespace lms::api::subsonic
{
    using namespace db;

    namespace
    {
        // Search endpoints can be used to scan/sync the database
        // This class is used to keep track of the current scans, in order to retrieve the last objectId
        // to speed up the query of the following range (avoid the 'offset' cost)
        template<typename ObjectId>
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

        private:
            using ClockType = std::chrono::steady_clock;

            struct Entry
            {
                ClockType::time_point timePoint;
                ObjectId objectId;
            };

            static constexpr std::size_t maxScanCount{ 50 };
            static constexpr ClockType::duration maxEntryDuration{ std::chrono::seconds{ 30 } };

            std::mutex _mutex;
            std::map<ScanInfo, Entry> _ongoingScans;
        };

        template<typename ObjectId>
        ObjectId ScanTracker<ObjectId>::extractLastRetrievedObjectId(const ScanInfo& scanInfo)
        {
            ObjectId res;

            {
                const std::scoped_lock lock{ _mutex };

                auto it{ _ongoingScans.find(scanInfo) };
                if (it != _ongoingScans.end())
                {
                    res = it->second.objectId;
                    _ongoingScans.erase(it);
                }
            }

            return res;
        }

        template<typename ObjectId>
        void ScanTracker<ObjectId>::setObjectId(const ScanInfo& scanInfo, ObjectId lastRetrievedId)
        {
            const ClockType::time_point now{ ClockType::now() };

            const std::scoped_lock lock{ _mutex };

            // clean outdated scan entries; we do this to not have to flush everything each time we add/remove entries in the database
            std::erase_if(_ongoingScans, [&](const auto& entry) { return now > entry.second.timePoint + maxEntryDuration; });
            // prevent the cache size from going out of control
            if (_ongoingScans.size() == maxScanCount)
                _ongoingScans.erase(core::random::pickRandom(_ongoingScans));

            _ongoingScans[scanInfo] = { now, lastRetrievedId };
        }

        void findRequestedArtists(RequestContext& context, bool id3, const std::vector<std::string_view>& keywords, MediaLibraryId mediaLibrary, const User::pointer& user, Response::Node& searchResultNode)
        {
            static ScanTracker<ArtistId> currentScansInProgress;

            const std::size_t artistCount{ getParameterAs<std::size_t>(context.parameters, "artistCount").value_or(20) };
            if (artistCount == 0)
                return;

            if (artistCount > defaultMaxCountSize)
                throw ParameterValueTooHighGenericError{ "artistCount", defaultMaxCountSize };

            const std::size_t artistOffset{ getParameterAs<std::size_t>(context.parameters, "artistOffset").value_or(0) };

            ArtistId lastRetrievedId;
            auto findArtists{ [&] {
                Artist::FindParameters params;
                params.setKeywords(keywords);
                params.setRange(Range{ artistOffset, artistCount });
                params.setMediaLibrary(mediaLibrary);
                params.setSortMethod(ArtistSortMethod::Id); // must be consistent with both methods

                Artist::find(context.dbSession, params, [&](const Artist::pointer& artist) {
                    searchResultNode.addArrayChild("artist", createArtistNode(context, artist, user, id3));
                    lastRetrievedId = artist->getId();
                });
            } };

            if (!keywords.empty())
            {
                findArtists();
            }
            else
            {
                ScanTracker<ArtistId>::ScanInfo scanInfo{
                    .clientAddress = context.clientInfo.ipAddress,
                    .clientName = context.clientInfo.name,
                    .userName = context.clientInfo.user,
                    .library = mediaLibrary,
                    .offset = artistOffset
                };

                if (ArtistId cachedLastRetrievedId{ currentScansInProgress.extractLastRetrievedObjectId(scanInfo) }; cachedLastRetrievedId.isValid())
                {
                    Artist::find(
                        context.dbSession, cachedLastRetrievedId, artistCount, [&](const Artist::pointer& artist) {
                            searchResultNode.addArrayChild("artist", createArtistNode(context, artist, user, id3));
                        },
                        mediaLibrary);
                    lastRetrievedId = cachedLastRetrievedId;
                }
                else
                {
                    findArtists();
                }

                if (lastRetrievedId.isValid())
                {
                    scanInfo.offset = artistOffset + artistCount;
                    currentScansInProgress.setObjectId(scanInfo, lastRetrievedId);
                }
            }
        }

        void findRequestedAlbums(RequestContext& context, bool id3, const std::vector<std::string_view>& keywords, MediaLibraryId mediaLibrary, const User::pointer& user, Response::Node& searchResultNode)
        {
            static ScanTracker<ReleaseId> currentScansInProgress;

            const std::size_t albumCount{ getParameterAs<std::size_t>(context.parameters, "albumCount").value_or(20) };
            if (albumCount == 0)
                return;

            if (albumCount > defaultMaxCountSize)
                throw ParameterValueTooHighGenericError{ "albumCount", defaultMaxCountSize };

            const std::size_t albumOffset{ getParameterAs<std::size_t>(context.parameters, "albumOffset").value_or(0) };

            ReleaseId lastRetrievedId;

            auto findReleases{ [&] {
                Release::FindParameters params;
                params.setKeywords(keywords);
                params.setRange(Range{ albumOffset, albumCount });
                params.setMediaLibrary(mediaLibrary);
                params.setSortMethod(ReleaseSortMethod::Id); // must be consistent with both methods

                Release::find(context.dbSession, params, [&](const Release::pointer& release) {
                    searchResultNode.addArrayChild("album", createAlbumNode(context, release, user, id3));
                    lastRetrievedId = release->getId();
                });
            } };

            if (!keywords.empty())
            {
                findReleases();
            }
            else
            {
                ScanTracker<ReleaseId>::ScanInfo scanInfo{
                    .clientAddress = context.clientInfo.ipAddress,
                    .clientName = context.clientInfo.name,
                    .userName = context.clientInfo.user,
                    .library = mediaLibrary,
                    .offset = albumOffset
                };

                if (ReleaseId cachedLastRetrievedId{ currentScansInProgress.extractLastRetrievedObjectId(scanInfo) }; cachedLastRetrievedId.isValid())
                {
                    Release::find(
                        context.dbSession, cachedLastRetrievedId, albumCount, [&](const Release::pointer& release) {
                            searchResultNode.addArrayChild("album", createAlbumNode(context, release, user, id3));
                        },
                        mediaLibrary);
                    lastRetrievedId = cachedLastRetrievedId;
                }
                else
                {
                    findReleases();
                }

                if (lastRetrievedId.isValid())
                {
                    scanInfo.offset = albumOffset + albumCount;
                    currentScansInProgress.setObjectId(scanInfo, lastRetrievedId);
                }
            }
        }

        void findRequestedTracks(RequestContext& context, const std::vector<std::string_view>& keywords, MediaLibraryId mediaLibrary, const User::pointer& user, Response::Node& searchResultNode)
        {
            static ScanTracker<TrackId> currentScansInProgress;

            const std::size_t songCount{ getParameterAs<std::size_t>(context.parameters, "songCount").value_or(20) };
            if (songCount == 0)
                return;

            if (songCount > defaultMaxCountSize)
                throw ParameterValueTooHighGenericError{ "songCount", defaultMaxCountSize };

            const std::size_t songOffset{ getParameterAs<std::size_t>(context.parameters, "songOffset").value_or(0) };

            TrackId lastRetrievedId;

            auto findTracks{ [&] {
                Track::FindParameters params;
                params.setKeywords(keywords);
                params.setRange(Range{ songOffset, songCount });
                params.setMediaLibrary(mediaLibrary);
                params.setSortMethod(TrackSortMethod::Id); // must be consistent with both methods

                Track::find(context.dbSession, params, [&](const Track::pointer& track) {
                    searchResultNode.addArrayChild("song", createSongNode(context, track, user));
                    lastRetrievedId = track->getId();
                });
            } };

            if (!keywords.empty())
            {
                findTracks();
            }
            else
            {
                ScanTracker<TrackId>::ScanInfo scanInfo{
                    .clientAddress = context.clientInfo.ipAddress,
                    .clientName = context.clientInfo.name,
                    .userName = context.clientInfo.user,
                    .library = mediaLibrary,
                    .offset = songOffset
                };

                if (TrackId cachedLastRetrievedId{ currentScansInProgress.extractLastRetrievedObjectId(scanInfo) }; cachedLastRetrievedId.isValid())
                {
                    Track::find(
                        context.dbSession, cachedLastRetrievedId, songCount, [&](const Track::pointer& track) {
                            searchResultNode.addArrayChild("song", createSongNode(context, track, user));
                        },
                        mediaLibrary);
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
    } // namespace

    namespace
    {
        Response handleSearchRequestCommon(RequestContext& context, bool id3)
        {
            // Mandatory params
            const std::string queryString{ getMandatoryParameterAs<std::string>(context.parameters, "query") };
            std::string_view query{ queryString };

            // Optional params
            const MediaLibraryId mediaLibrary{ getParameterAs<MediaLibraryId>(context.parameters, "musicFolderId").value_or(MediaLibraryId{}) };

            // Symfonium adds extra ""
            if (context.clientInfo.name == "Symfonium")
                query = core::stringUtils::stringTrim(query, "\"");

            std::vector<std::string_view> keywords;
            if (!query.empty())
                keywords = core::stringUtils::splitString(query, ' ');

            Response response{ Response::createOkResponse(context.serverProtocolVersion) };
            Response::Node& searchResultNode{ response.createNode(id3 ? "searchResult3" : "searchResult2") };

            auto transaction{ context.dbSession.createReadTransaction() };

            findRequestedArtists(context, id3, keywords, mediaLibrary, context.user, searchResultNode);
            findRequestedAlbums(context, id3, keywords, mediaLibrary, context.user, searchResultNode);
            findRequestedTracks(context, keywords, mediaLibrary, context.user, searchResultNode);

            return response;
        }
    } // namespace

    Response handleSearch2Request(RequestContext& context)
    {
        return handleSearchRequestCommon(context, false /* no id3 */);
    }

    Response handleSearch3Request(RequestContext& context)
    {
        return handleSearchRequestCommon(context, true /* id3 */);
    }
} // namespace lms::api::subsonic