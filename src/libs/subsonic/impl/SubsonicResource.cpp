/*
 * copyright (c) 2019 emeric poupon
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

#include "SubsonicResource.hpp"

#include <atomic>
#include <unordered_map>

#include "core/EnumSet.hpp"
#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "core/LiteralString.hpp"
#include "core/Service.hpp"
#include "core/String.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/User.hpp"
#include "services/auth/IAuthTokenService.hpp"
#include "services/auth/IPasswordService.hpp"

#include "ParameterParsing.hpp"
#include "RequestContext.hpp"
#include "SubsonicResponse.hpp"
#include "endpoints/AlbumSongLists.hpp"
#include "endpoints/Bookmarks.hpp"
#include "endpoints/Browsing.hpp"
#include "endpoints/Jukebox.hpp"
#include "endpoints/MediaAnnotation.hpp"
#include "endpoints/MediaLibraryScanning.hpp"
#include "endpoints/MediaRetrieval.hpp"
#include "endpoints/Playlists.hpp"
#include "endpoints/Podcast.hpp"
#include "endpoints/Searching.hpp"
#include "endpoints/System.hpp"
#include "endpoints/Transcoding.hpp"
#include "endpoints/UserManagement.hpp"

namespace lms::api::subsonic
{
    std::unique_ptr<Wt::WResource> createSubsonicResource(db::IDb& db)
    {
        return std::make_unique<SubsonicResource>(db);
    }

    namespace
    {
        std::string parameterMapToDebugString(const Wt::Http::ParameterMap& parameterMap)
        {
            constexpr std::string_view redactedStr{ "*REDACTED*" };
            auto redactValueIfNeeded = [redactedStr](const std::string& type, const std::string& value) -> std::string_view {
                if (type == "p" || type == "password" || type == "apiKey")
                    return redactedStr;

                return value;
            };

            std::string res;

            bool firstParameter{ true };
            for (const auto& [type, values] : parameterMap)
            {
                if (!firstParameter)
                    res += ", ";
                firstParameter = false;

                res += "{" + type + "=";
                if (values.size() == 1)
                {
                    res += redactValueIfNeeded(type, values.front());
                }
                else
                {
                    res += "{";
                    bool firstValue{ true };
                    for (const std::string& value : values)
                    {
                        if (!firstValue)
                            res += ',';
                        firstValue = false;

                        res += redactValueIfNeeded(type, value);
                    }
                    res += "}";
                }
                res += "}";
            }

            return res;
        }

        void checkUserTypeIsAllowed(const db::User::pointer& user, core::EnumSet<db::UserType> allowedUserTypes)
        {
            assert(user);
            if (!allowedUserTypes.contains(user->getType()))
                throw UserNotAuthorizedError{};
        }

        Response handleNotImplemented(RequestContext& /*context*/)
        {
            throw NotImplementedGenericError{};
        }

        enum class AuthenticationMode
        {
            Authenticated,
            Unauthenticated,
        };
        using RequestHandlerFunc = std::function<Response(RequestContext& context)>;
        struct RequestEntryPointInfo
        {
            RequestHandlerFunc func;
            AuthenticationMode authMode{ AuthenticationMode::Authenticated };
            core::EnumSet<db::UserType> allowedUserTypes{ db::UserType::DEMO, db::UserType::REGULAR, db::UserType::ADMIN };
        };

        const std::unordered_map<core::LiteralString, RequestEntryPointInfo, core::LiteralStringHash, core::LiteralStringEqual> requestEntryPoints{
            // System
            { "/ping", { handlePingRequest } },
            { "/getLicense", { handleGetLicenseRequest } },
            { "/getOpenSubsonicExtensions", { handleGetOpenSubsonicExtensions, AuthenticationMode::Unauthenticated } },

            // Browsing
            { "/getMusicFolders", { handleGetMusicFoldersRequest } },
            { "/getIndexes", { handleGetIndexesRequest } },
            { "/getMusicDirectory", { handleGetMusicDirectoryRequest } },
            { "/getGenres", { handleGetGenresRequest } },
            { "/getArtists", { handleGetArtistsRequest } },
            { "/getArtist", { handleGetArtistRequest } },
            { "/getAlbum", { handleGetAlbumRequest } },
            { "/getSong", { handleGetSongRequest } },
            { "/getVideos", { handleNotImplemented } },
            { "/getArtistInfo", { handleNotImplemented } },
            { "/getArtistInfo2", { handleGetArtistInfo2Request } },
            { "/getAlbumInfo", { handleGetAlbumInfo } },
            { "/getAlbumInfo2", { handleGetAlbumInfo2 } },
            { "/getSimilarSongs", { handleGetSimilarSongsRequest } },
            { "/getSimilarSongs2", { handleGetSimilarSongs2Request } },
            { "/getTopSongs", { handleGetTopSongs } },
            { "/getSonicSimilarTracks", { handleGetSonicSimilarTracksRequest } },
            { "/findSonicPath", { handleFindSonicPathRequest } },

            // Album/song lists
            { "/getAlbumList", { handleGetAlbumListRequest } },
            { "/getAlbumList2", { handleGetAlbumList2Request } },
            { "/getRandomSongs", { handleGetRandomSongsRequest } },
            { "/getSongsByGenre", { handleGetSongsByGenreRequest } },
            { "/getNowPlaying", { handleGetNowPlayingRequest } },
            { "/getStarred", { handleGetStarredRequest } },
            { "/getStarred2", { handleGetStarred2Request } },

            // Searching
            { "/search", { handleNotImplemented } },
            { "/search2", { handleSearch2Request } },
            { "/search3", { handleSearch3Request } },

            // Playlists
            { "/getPlaylists", { handleGetPlaylistsRequest } },
            { "/getPlaylist", { handleGetPlaylistRequest } },
            { "/createPlaylist", { handleCreatePlaylistRequest } },
            { "/updatePlaylist", { handleUpdatePlaylistRequest } },
            { "/deletePlaylist", { handleDeletePlaylistRequest } },

            // Media retrieval
            { "/hls", { handleNotImplemented } },
            { "/getCaptions", { handleNotImplemented } },
            { "/getLyrics", { handleGetLyrics } },
            { "/getLyricsBySongId", { handleGetLyricsBySongId } },
            { "/getAvatar", { handleNotImplemented } },

            // Transcoding extensions
            { "/getTranscodeDecision", { handleGetTranscodeDecision } },

            // Media annotation
            { "/star", { handleStarRequest } },
            { "/unstar", { handleUnstarRequest } },
            { "/setRating", { handleSetRating } },
            { "/scrobble", { handleScrobble } },

            // Sharing
            { "/getShares", { handleNotImplemented } },
            { "/createShares", { handleNotImplemented } },
            { "/updateShare", { handleNotImplemented } },
            { "/deleteShare", { handleNotImplemented } },

            // Podcast
            { "/getPodcasts", { handleGetPodcasts } },
            { "/getNewestPodcasts", { handleGetNewestPodcasts } },
            { "/refreshPodcasts", { handleRefreshPodcasts, AuthenticationMode::Authenticated, { db::UserType::ADMIN } } },
            { "/createPodcastChannel", { handleCreatePodcastChannel, AuthenticationMode::Authenticated, { db::UserType::ADMIN } } },
            { "/deletePodcastChannel", { handleDeletePodcastChannel, AuthenticationMode::Authenticated, { db::UserType::ADMIN } } },
            { "/deletePodcastEpisode", { handleDeletePodcastEpisode, AuthenticationMode::Authenticated, { db::UserType::ADMIN } } },
            { "/downloadPodcastEpisode", { handleDownloadPodcastEpisode, AuthenticationMode::Authenticated, { db::UserType::ADMIN } } },
            { "/getPodcastEpisode", { handleGetPodcastEpisode } },

            // Jukebox
            { "/jukeboxControl", { handleJukeboxControl } },

            // Internet radio
            { "/getInternetRadioStations", { handleNotImplemented } },
            { "/createInternetRadioStation", { handleNotImplemented } },
            { "/updateInternetRadioStation", { handleNotImplemented } },
            { "/deleteInternetRadioStation", { handleNotImplemented } },

            // Chat
            { "/getChatMessages", { handleNotImplemented } },
            { "/addChatMessages", { handleNotImplemented } },

            // User management
            { "/getUser", { handleGetUserRequest } },
            { "/getUsers", { handleGetUsersRequest, AuthenticationMode::Authenticated, { db::UserType::ADMIN } } },
            { "/createUser", { handleNotImplemented } },
            { "/updateUser", { handleNotImplemented } },
            { "/deleteUser", { handleNotImplemented } },
            { "/changePassword", { handleNotImplemented } },

            // Bookmarks
            { "/getBookmarks", { handleGetBookmarks } },
            { "/createBookmark", { handleCreateBookmark } },
            { "/deleteBookmark", { handleDeleteBookmark } },
            { "/getPlayQueue", { handleGetPlayQueue } },
            { "/savePlayQueue", { handleSavePlayQueue } },
            { "/getPlayQueueByIndex", { handleGetPlayQueueByIndex } },
            { "/savePlayQueueByIndex", { handleSavePlayQueueByIndex } },

            // Media library scanning
            { "/getScanStatus", { Scan::handleGetScanStatus } },
            { "/startScan", { Scan::handleStartScan, AuthenticationMode::Authenticated, { db::UserType::ADMIN } } },
        };

        using MediaRetrievalHandlerFunc = std::function<void(RequestContext&, const Wt::Http::Request&, Wt::Http::Response&)>;
        const std::unordered_map<core::LiteralString, MediaRetrievalHandlerFunc, core::LiteralStringHash, core::LiteralStringEqual> mediaRetrievalHandlers{
            // Media retrieval
            { "/download", handleDownload },
            { "/stream", handleStream },
            { "/getCoverArt", handleGetCoverArt },

            // Transcoding extension
            { "/getTranscodeStream", handleGetTranscodeStream },
        };

        struct TLSMonotonicMemoryResourceCleaner
        {
            TLSMonotonicMemoryResourceCleaner() = default;
            ~TLSMonotonicMemoryResourceCleaner()
            {
                TLSMonotonicMemoryResource::getInstance().reset();
            }

        private:
            TLSMonotonicMemoryResourceCleaner(const TLSMonotonicMemoryResourceCleaner&) = delete;
            TLSMonotonicMemoryResourceCleaner& operator=(const TLSMonotonicMemoryResourceCleaner&) = delete;
        };

        db::User::pointer getUserFromUserId(db::Session& session, db::UserId userId)
        {
            auto transaction{ session.createReadTransaction() };

            if (db::User::pointer user{ db::User::find(session, userId) })
                return user;

            throw UserNotAuthorizedError{};
        }
    } // namespace

    SubsonicResource::SubsonicResource(db::IDb& db)
        : _config{ readSubsonicResourceConfig(*core::Service<core::IConfig>::get()) }
        , _db{ db }
    {
    }

    void SubsonicResource::handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response)
    {
        static std::atomic<std::size_t> curRequestId{};

        const std::size_t requestId{ curRequestId++ };
        TLSMonotonicMemoryResourceCleaner memoryResourceCleaner;

        constexpr std::string_view optionalSuffix{ ".view" };
        std::string requestPath{ request.pathInfo() };
        if (core::stringUtils::stringEndsWith(requestPath, optionalSuffix))
            requestPath.resize(requestPath.length() - optionalSuffix.size());

        LMS_LOG(API_SUBSONIC, DEBUG, "Handling request " << requestId << " to '" << requestPath << " with params = " << parameterMapToDebugString(request.getParameterMap()) << "', continuation = " << (request.continuation() ? "true" : "false"));

        try
        {
            if (!handleMediaRetrievalRequest(requestPath, request, response))
                handleRequest(requestPath, request, response);

            LMS_LOG(API_SUBSONIC, DEBUG, "Request " << requestId << " to '" << requestPath << "' handled!");
        }
        catch (const Error& e)
        {
            LMS_LOG(API_SUBSONIC, ERROR, "Error while processing request " << requestId << " to '" << requestPath << "' with params = " << parameterMapToDebugString(request.getParameterMap()) << ": code = " << static_cast<int>(e.getCode()) << ", msg = '" << e.getMessage() << "'");
        }
    }

    bool SubsonicResource::handleMediaRetrievalRequest(const std::string& requestPath, const Wt::Http::Request& request, Wt::Http::Response& response)
    {
        auto itStreamHandler{ mediaRetrievalHandlers.find(requestPath) };
        if (itStreamHandler == mediaRetrievalHandlers.end())
            return false;

        LMS_SCOPED_TRACE_OVERVIEW("Subsonic", itStreamHandler->first);

        try
        {
            RequestContext requestContext{ request, _db.getTLSSession(), _config };

            // Media retrieval endpoints are always authenticated but we don't reauth user for a continuation
            db::User::pointer user;
            if (!request.continuation())
                user = getUserFromUserId(_db.getTLSSession(), authenticateUser(request));

            requestContext.setUser(user);

            itStreamHandler->second(requestContext, request, response);

            return true;
        }
        catch (const UserNotAuthorizedError&)
        {
            response.setStatus(401); // Unauthorized
            throw;
        }
        catch (const RequiredParameterMissingError&)
        {
            response.setStatus(400); // Bad Request
            throw;
        }
        catch (const BadParameterGenericError&)
        {
            response.setStatus(400); // Bad Request
            throw;
        }
        catch (const RequestedDataNotFoundError&)
        {
            response.setStatus(404); // Not Found
            throw;
        }
        catch (const InternalErrorGenericError&)
        {
            response.setStatus(500); // Internal Server Error
            throw;
        }
        catch (const Error&)
        {
            response.setStatus(400); // Assume bad request
            throw;
        }
    }

    void SubsonicResource::handleRequest(const std::string& requestPath, const Wt::Http::Request& request, Wt::Http::Response& response)
    {
        auto writeResponse{ [&](const Response& resp, ResponseFormat format) {
            LMS_SCOPED_TRACE_DETAILED("Subsonic", "WriteResponse");
            resp.write(response.out(), format);
            response.setMimeType(std::string{ ResponseFormatToMimeType(format) });
        } };

        std::optional<RequestContext> requestContext;
        try
        {
            requestContext.emplace(request, _db.getTLSSession(), _config);
        }
        catch (const Error& e)
        {
            writeResponse(Response::createFailedResponse(defaultServerProtocolVersion, e), ResponseFormat::xml);
            throw;
        }

        try
        {
            if (auto itEntryPoint{ requestEntryPoints.find(requestPath) }; itEntryPoint != requestEntryPoints.end())
            {
                LMS_SCOPED_TRACE_OVERVIEW("Subsonic", itEntryPoint->first);

                db::User::pointer user;
                if (itEntryPoint->second.authMode == AuthenticationMode::Authenticated)
                {
                    user = getUserFromUserId(_db.getTLSSession(), authenticateUser(request));
                    checkUserTypeIsAllowed(user, itEntryPoint->second.allowedUserTypes);
                    requestContext->setUser(user);
                }

                const Response resp{ [&] {
                    LMS_SCOPED_TRACE_DETAILED("Subsonic", "HandleRequest");
                    return itEntryPoint->second.func(*requestContext);
                }() };

                writeResponse(resp, requestContext->getResponseFormat());
                return;
            }
            // do not disclose unhandled commands for unauthenticated users
            authenticateUser(request);

            LMS_LOG(API_SUBSONIC, ERROR, "Unhandled command '" << requestPath << "'");
            throw UnknownEntryPointGenericError{};
        }
        catch (const Error& e)
        {
            Response resp{ Response::createFailedResponse(requestContext->getServerProtocolVersion(), e) };
            writeResponse(resp, requestContext->getResponseFormat());
            throw;
        }
    }

    db::UserId SubsonicResource::authenticateUser(const Wt::Http::Request& request)
    {
        const auto& parameters{ request.getParameterMap() };

        if (hasParameter(parameters, "t"))
            throw ProvidedAuthenticationMechanismNotSupportedError{};

        const auto user{ getParameterAs<std::string>(parameters, "u") };
        const auto password{ getParameterAs<std::string>(parameters, "p") };
        if (!_config.supportUserPasswordAuthentication && (password || user))
            throw ProvidedAuthenticationMechanismNotSupportedError{};

        const auto apiKey{ getParameterAs<std::string>(parameters, "apiKey") };

        if (user && !password)
            throw RequiredParameterMissingError{ "p" };
        if (!user && password)
            throw RequiredParameterMissingError{ "u" };
        if (apiKey && password)
            throw MultipleConflictingAuthenticationMechanismsProvidedError{};
        if (!apiKey && !password)
            throw RequiredParameterMissingError{ "apiKey" };

        const auto clientAddress{ boost::asio::ip::make_address(request.clientAddress()) };
        const std::string authToken{ apiKey ? *apiKey : decodePasswordIfNeeded(*password) };

        const auto authResult{ core::Service<auth::IAuthTokenService>::get()->processAuthToken("subsonic", clientAddress, authToken) };
        switch (authResult.state)
        {
        case auth::IAuthTokenService::AuthTokenProcessResult::State::Granted:
            if (user)
            {
                const auto authenticatedUser{ getUserFromUserId(_db.getTLSSession(), authResult.authTokenInfo->userId) };
                if (!authenticatedUser || authenticatedUser->getLoginName() != *user)
                    throw WrongUsernameOrPasswordError{};
            }
            return authResult.authTokenInfo->userId;
        case auth::IAuthTokenService::AuthTokenProcessResult::State::Denied:
            if (apiKey)
                throw InvalidAPIkeyError{};
            else
                throw WrongUsernameOrPasswordError{};
        case auth::IAuthTokenService::AuthTokenProcessResult::State::Throttled:
            throw LoginThrottledGenericError{};
        }

        throw InternalErrorGenericError{ "Cannot authenticate user" };
    }
} // namespace lms::api::subsonic
