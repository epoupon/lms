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
#include "ProtocolVersion.hpp"
#include "RequestContext.hpp"
#include "SubsonicResponse.hpp"
#include "endpoints/AlbumSongLists.hpp"
#include "endpoints/Bookmarks.hpp"
#include "endpoints/Browsing.hpp"
#include "endpoints/MediaAnnotation.hpp"
#include "endpoints/MediaLibraryScanning.hpp"
#include "endpoints/MediaRetrieval.hpp"
#include "endpoints/Playlists.hpp"
#include "endpoints/Searching.hpp"
#include "endpoints/System.hpp"
#include "endpoints/UserManagement.hpp"

namespace lms::api::subsonic
{
    std::unique_ptr<Wt::WResource> createSubsonicResource(db::IDb& db)
    {
        return std::make_unique<SubsonicResource>(db);
    }

    namespace
    {
        std::unordered_map<std::string, ProtocolVersion> readConfigProtocolVersions()
        {
            std::unordered_map<std::string, ProtocolVersion> res;

            core::Service<core::IConfig>::get()->visitStrings("api-subsonic-old-server-protocol-clients",
                [&](std::string_view client) {
                    res.emplace(std::string{ client }, ProtocolVersion{ .major = 1, .minor = 12, .patch = 0 });
                },
                { "DSub" });

            return res;
        }

        std::unordered_set<std::string> readOpenSubsonicDisabledClients()
        {
            std::unordered_set<std::string> res;

            core::Service<core::IConfig>::get()->visitStrings("api-open-subsonic-disabled-clients",
                [&](std::string_view client) {
                    res.emplace(std::string{ client });
                },
                { "DSub" });

            return res;
        }

        std::string parameterMapToDebugString(const Wt::Http::ParameterMap& parameterMap)
        {
            constexpr std::string_view redactedStr{ "*REDACTED*" };
            auto redactValueIfNeeded = [redactedStr](const std::string& type, const std::string& value) -> std::string_view {
                if (type == "p" || type == "password" || type == "apiKey")
                    return redactedStr;

                return value;
            };

            std::string res;

            for (const auto& [type, values] : parameterMap)
            {
                res += "{" + type + "=";
                if (values.size() == 1)
                {
                    res += redactValueIfNeeded(type, values.front());
                }
                else
                {
                    res += "{";
                    for (const std::string& value : values)
                    {
                        res += redactValueIfNeeded(type, value);
                        res += ',';
                    }
                    res += "}";
                }
                res += "}, ";
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

            // Album/song lists
            { "/getAlbumList", { handleGetAlbumListRequest } },
            { "/getAlbumList2", { handleGetAlbumList2Request } },
            { "/getRandomSongs", { handleGetRandomSongsRequest } },
            { "/getSongsByGenre", { handleGetSongsByGenreRequest } },
            { "/getNowPlaying", { handleNotImplemented } },
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
            { "/getPodcasts", { handleNotImplemented } },
            { "/getNewestPodcasts", { handleNotImplemented } },
            { "/refreshPodcasts", { handleNotImplemented } },
            { "/createPodcastChannel", { handleNotImplemented } },
            { "/deletePodcastChannel", { handleNotImplemented } },
            { "/deletePodcastEpisode", { handleNotImplemented } },
            { "/downloadPodcastEpisode", { handleNotImplemented } },

            // Jukebox
            { "/jukeboxControl", { handleNotImplemented } },

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

        ClientInfo getClientInfo(const Wt::Http::Request& request)
        {
            const auto& parameters{ request.getParameterMap() };
            ClientInfo res;

            // Mandatory parameters
            res.name = getMandatoryParameterAs<std::string>(parameters, "c");
            res.version = getMandatoryParameterAs<ProtocolVersion>(parameters, "v");

            return res;
        }
    } // namespace

    SubsonicResource::SubsonicResource(db::IDb& db)
        : _serverProtocolVersionsByClient{ readConfigProtocolVersions() }
        , _openSubsonicDisabledClients{ readOpenSubsonicDisabledClients() }
        , _supportUserPasswordAuthentication{ core::Service<core::IConfig>::get()->getBool("api-subsonic-support-user-password-auth", true) }
        , _db{ db }
    {
    }

    void SubsonicResource::handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response)
    {
        static std::atomic<std::size_t> curRequestId{};

        const std::size_t requestId{ curRequestId++ };
        TLSMonotonicMemoryResourceCleaner memoryResourceCleaner;

        LMS_LOG(API_SUBSONIC, DEBUG, "Handling request " << requestId << " '" << request.pathInfo() << "', continuation = " << (request.continuation() ? "true" : "false") << ", params = " << parameterMapToDebugString(request.getParameterMap()));

        std::string requestPath{ request.pathInfo() };
        if (core::stringUtils::stringEndsWith(requestPath, ".view"))
            requestPath.resize(requestPath.length() - 5);

        // Optional parameters
        const ResponseFormat format{ getParameterAs<std::string>(request.getParameterMap(), "f").value_or("xml") == "json" ? ResponseFormat::json : ResponseFormat::xml };

        ProtocolVersion protocolVersion{ defaultServerProtocolVersion };

        try
        {
            // We need to parse client a soon as possible to make sure to answer with the right protocol version
            protocolVersion = getServerProtocolVersion(getMandatoryParameterAs<std::string>(request.getParameterMap(), "c"));
            RequestContext requestContext{ buildRequestContext(request) };

            auto itEntryPoint{ requestEntryPoints.find(requestPath) };
            if (itEntryPoint != requestEntryPoints.end())
            {
                LMS_SCOPED_TRACE_OVERVIEW("Subsonic", itEntryPoint->first);

                if (itEntryPoint->second.authMode == AuthenticationMode::Authenticated)
                {
                    requestContext.user = getUserFromUserId(_db.getTLSSession(), authenticateUser(request));
                    checkUserTypeIsAllowed(requestContext.user, itEntryPoint->second.allowedUserTypes);
                }

                const Response resp{ [&] {
                    LMS_SCOPED_TRACE_DETAILED("Subsonic", "HandleRequest");
                    return itEntryPoint->second.func(requestContext);
                }() };

                {
                    LMS_SCOPED_TRACE_DETAILED("Subsonic", "WriteResponse");

                    resp.write(response.out(), format);
                    response.setMimeType(std::string{ ResponseFormatToMimeType(format) });
                }

                LMS_LOG(API_SUBSONIC, DEBUG, "Request " << requestId << " '" << requestPath << "' handled!");
                return;
            }

            auto itStreamHandler{ mediaRetrievalHandlers.find(requestPath) };
            if (itStreamHandler != mediaRetrievalHandlers.end())
            {
                LMS_SCOPED_TRACE_OVERVIEW("Subsonic", itStreamHandler->first);

                // Media retrieval endpoints are always authenticated
                // Optim: no need to reauth user for each continuation
                if (!request.continuation())
                    requestContext.user = getUserFromUserId(_db.getTLSSession(), authenticateUser(request));

                itStreamHandler->second(requestContext, request, response);
                LMS_LOG(API_SUBSONIC, DEBUG, "Request " << requestId << " '" << requestPath << "' handled!");
                return;
            }

            // do not disclose unhandled commands for unauthenticated users
            authenticateUser(request);

            LMS_LOG(API_SUBSONIC, ERROR, "Unhandled command '" << requestPath << "'");
            throw UnknownEntryPointGenericError{};
        }
        catch (const Error& e)
        {
            LMS_LOG(API_SUBSONIC, ERROR, "Error while processing request '" << requestPath << "'"
                                                                            << ", params = [" << parameterMapToDebugString(request.getParameterMap()) << "]"
                                                                            << ", code = " << static_cast<int>(e.getCode()) << ", msg = '" << e.getMessage() << "'");
            Response resp{ Response::createFailedResponse(protocolVersion, e) };
            resp.write(response.out(), format);
            response.setMimeType(std::string{ ResponseFormatToMimeType(format) });
        }
    }

    ProtocolVersion SubsonicResource::getServerProtocolVersion(const std::string& clientName) const
    {
        auto it{ _serverProtocolVersionsByClient.find(clientName) };
        if (it == std::cend(_serverProtocolVersionsByClient))
            return defaultServerProtocolVersion;

        return it->second;
    }

    void SubsonicResource::checkProtocolVersion(ProtocolVersion client, ProtocolVersion server)
    {
        if (client.major > server.major)
            throw ServerMustUpgradeError{};
        if (client.major < server.major)
            throw ClientMustUpgradeError{};
        if (client.minor > server.minor)
            throw ServerMustUpgradeError{};
        if (client.minor == server.minor)
        {
            if (client.patch > server.patch)
                throw ServerMustUpgradeError{};
        }
    }

    RequestContext SubsonicResource::buildRequestContext(const Wt::Http::Request& request)
    {
        const Wt::Http::ParameterMap& parameters{ request.getParameterMap() };
        const ClientInfo clientInfo{ getClientInfo(request) };
        bool enableOpenSubsonic{ !_openSubsonicDisabledClients.contains(clientInfo.name) };
        const ResponseFormat format{ getParameterAs<std::string>(request.getParameterMap(), "f").value_or("xml") == "json" ? ResponseFormat::json : ResponseFormat::xml };

        return RequestContext{
            .parameters = parameters,
            .dbSession = _db.getTLSSession(),
            .user = db::User::pointer{},
            .clientIpAddr = request.clientAddress(),
            .clientInfo = clientInfo,
            .serverProtocolVersion = getServerProtocolVersion(clientInfo.name),
            .responseFormat = format,
            .enableOpenSubsonic = enableOpenSubsonic,
        };
    }

    db::UserId SubsonicResource::authenticateUser(const Wt::Http::Request& request)
    {
        const auto& parameters{ request.getParameterMap() };

        if (hasParameter(parameters, "t"))
            throw ProvidedAuthenticationMechanismNotSupportedError{};

        const auto user{ getParameterAs<std::string>(parameters, "u") };
        const auto password{ getParameterAs<std::string>(parameters, "p") };
        if (!_supportUserPasswordAuthentication && (password || user))
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
