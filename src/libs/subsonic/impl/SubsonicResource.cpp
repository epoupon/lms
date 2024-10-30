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
#include "core/Utils.hpp"
#include "database/Db.hpp"
#include "database/Session.hpp"
#include "database/User.hpp"
#include "services/auth/IEnvService.hpp"
#include "services/auth/IPasswordService.hpp"

#include "ParameterParsing.hpp"
#include "ProtocolVersion.hpp"
#include "RequestContext.hpp"
#include "SubsonicId.hpp"
#include "SubsonicResponse.hpp"
#include "Utils.hpp"
#include "entrypoints/AlbumSongLists.hpp"
#include "entrypoints/Bookmarks.hpp"
#include "entrypoints/Browsing.hpp"
#include "entrypoints/MediaAnnotation.hpp"
#include "entrypoints/MediaLibraryScanning.hpp"
#include "entrypoints/MediaRetrieval.hpp"
#include "entrypoints/Playlists.hpp"
#include "entrypoints/Searching.hpp"
#include "entrypoints/System.hpp"
#include "entrypoints/UserManagement.hpp"

namespace lms::api::subsonic
{
    std::unique_ptr<Wt::WResource> createSubsonicResource(db::Db& db)
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
                    res.emplace(std::string{ client }, ProtocolVersion{ 1, 12, 0 });
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

        std::unordered_set<std::string> readDefaultCoverClients()
        {
            std::unordered_set<std::string> res;

            core::Service<core::IConfig>::get()->visitStrings("api-subsonic-default-cover-clients",
                [&](std::string_view client) {
                    res.emplace(std::string{ client });
                },
                { "DSub", "substreamer" });

            return res;
        }

        std::string parameterMapToDebugString(const Wt::Http::ParameterMap& parameterMap)
        {
            auto censorValue = [](const std::string& type, const std::string& value) -> std::string {
                if (type == "p" || type == "password")
                    return "*REDACTED*";
                else
                    return value;
            };

            std::string res;

            for (const auto& params : parameterMap)
            {
                res += "{" + params.first + "=";
                if (params.second.size() == 1)
                {
                    res += censorValue(params.first, params.second.front());
                }
                else
                {
                    res += "{";
                    for (const std::string& param : params.second)
                        res += censorValue(params.first, param) + ",";
                    res += "}";
                }
                res += "}, ";
            }

            return res;
        }

        void checkUserTypeIsAllowed(RequestContext& context, core::EnumSet<db::UserType> allowedUserTypes)
        {
            if (!allowedUserTypes.contains(context.user->getType()))
                throw UserNotAuthorizedError{};
        }

        Response handleNotImplemented(RequestContext&)
        {
            throw NotImplementedGenericError{};
        }

        using RequestHandlerFunc = std::function<Response(RequestContext& context)>;
        using CheckImplementedFunc = std::function<void()>;
        struct RequestEntryPointInfo
        {
            RequestHandlerFunc func;
            core::EnumSet<db::UserType> allowedUserTypes{ db::UserType::DEMO, db::UserType::REGULAR, db::UserType::ADMIN };
            CheckImplementedFunc checkFunc{};
        };

        const std::unordered_map<core::LiteralString, RequestEntryPointInfo, core::LiteralStringHash, core::LiteralStringEqual> requestEntryPoints{
            // System
            { "/ping", { handlePingRequest } },
            { "/getLicense", { handleGetLicenseRequest } },
            { "/getOpenSubsonicExtensions", { handleGetOpenSubsonicExtensions } },

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
            { "/getAlbumInfo", { handleNotImplemented } },
            { "/getAlbumInfo2", { handleNotImplemented } },
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
            { "/getUsers", { handleGetUsersRequest, { db::UserType::ADMIN } } },
            { "/createUser", { handleCreateUserRequest, { db::UserType::ADMIN }, &utils::checkSetPasswordImplemented } },
            { "/updateUser", { handleUpdateUserRequest, { db::UserType::ADMIN } } },
            { "/deleteUser", { handleDeleteUserRequest, { db::UserType::ADMIN } } },
            { "/changePassword", { handleChangePassword, { db::UserType::REGULAR, db::UserType::ADMIN }, &utils::checkSetPasswordImplemented } },

            // Bookmarks
            { "/getBookmarks", { handleGetBookmarks } },
            { "/createBookmark", { handleCreateBookmark } },
            { "/deleteBookmark", { handleDeleteBookmark } },
            { "/getPlayQueue", { handleNotImplemented } },
            { "/savePlayQueue", { handleNotImplemented } },

            // Media library scanning
            { "/getScanStatus", { Scan::handleGetScanStatus, { db::UserType::ADMIN } } },
            { "/startScan", { Scan::handleStartScan, { db::UserType::ADMIN } } },
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
    } // namespace

    SubsonicResource::SubsonicResource(db::Db& db)
        : _serverProtocolVersionsByClient{ readConfigProtocolVersions() }
        , _openSubsonicDisabledClients{ readOpenSubsonicDisabledClients() }
        , _defaultReleaseCoverClients{ readDefaultCoverClients() }
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

                if (itEntryPoint->second.checkFunc)
                    itEntryPoint->second.checkFunc();

                checkUserTypeIsAllowed(requestContext, itEntryPoint->second.allowedUserTypes);

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

                itStreamHandler->second(requestContext, request, response);
                LMS_LOG(API_SUBSONIC, DEBUG, "Request " << requestId << " '" << requestPath << "' handled!");
                return;
            }

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
        else if (client.minor == server.minor)
        {
            if (client.patch > server.patch)
                throw ServerMustUpgradeError{};
        }
    }

    ClientInfo SubsonicResource::getClientInfo(const Wt::Http::Request& request)
    {
        const auto& parameters{ request.getParameterMap() };
        ClientInfo res;

        if (hasParameter(parameters, "t"))
            throw TokenAuthenticationNotSupportedForLDAPUsersError{};

        res.ipAddress = request.clientAddress();

        // Mandatory parameters
        res.name = getMandatoryParameterAs<std::string>(parameters, "c");
        res.version = getMandatoryParameterAs<ProtocolVersion>(parameters, "v");
        res.user = getMandatoryParameterAs<std::string>(parameters, "u");
        res.password = decodePasswordIfNeeded(getMandatoryParameterAs<std::string>(parameters, "p"));

        return res;
    }

    RequestContext SubsonicResource::buildRequestContext(const Wt::Http::Request& request)
    {
        const Wt::Http::ParameterMap& parameters{ request.getParameterMap() };
        const ClientInfo clientInfo{ getClientInfo(request) };
        const db::UserId userId{ authenticateUser(request, clientInfo) };
        bool enableOpenSubsonic{ !_openSubsonicDisabledClients.contains(clientInfo.name) };
        bool enableDefaultCover{ _defaultReleaseCoverClients.contains(clientInfo.name) };
        const ResponseFormat format{ getParameterAs<std::string>(request.getParameterMap(), "f").value_or("xml") == "json" ? ResponseFormat::json : ResponseFormat::xml };

        db::User::pointer user;
        {
            db::Session& session{ _db.getTLSSession() };
            auto transaction{ session.createReadTransaction() };

            user = db::User::find(session, userId);
            if (!user)
                throw UserNotAuthorizedError{};
        }

        return RequestContext{
            .parameters = parameters,
            .dbSession = _db.getTLSSession(),
            .user = user,
            .clientInfo = clientInfo,
            .serverProtocolVersion = getServerProtocolVersion(clientInfo.name),
            .responseFormat = format,
            .enableOpenSubsonic = enableOpenSubsonic,
            .enableDefaultCover = enableDefaultCover
        };
    }

    db::UserId SubsonicResource::authenticateUser(const Wt::Http::Request& request, const ClientInfo& clientInfo)
    {
        // if the request if a continuation, the user is already authenticated
        if (request.continuation())
        {
            db::Session& session{ _db.getTLSSession() };
            auto transaction{ session.createReadTransaction() };

            const auto user{ db::User::find(session, clientInfo.user) };
            if (!user)
                throw UserNotAuthorizedError{};

            return user->getId();
        }

        if (auto* authEnvService{ core::Service<auth::IEnvService>::get() })
        {
            const auto checkResult{ authEnvService->processRequest(request) };
            if (checkResult.state != auth::IEnvService::CheckResult::State::Granted)
                throw UserNotAuthorizedError{};

            return *checkResult.userId;
        }
        else if (auto* authPasswordService{ core::Service<auth::IPasswordService>::get() })
        {
            const auto checkResult{ authPasswordService->checkUserPassword(boost::asio::ip::address::from_string(request.clientAddress()), clientInfo.user, clientInfo.password) };

            switch (checkResult.state)
            {
            case auth::IPasswordService::CheckResult::State::Granted:
                return *checkResult.userId;
                break;
            case auth::IPasswordService::CheckResult::State::Denied:
                throw WrongUsernameOrPasswordError{};
            case auth::IPasswordService::CheckResult::State::Throttled:
                throw LoginThrottledGenericError{};
            }
        }

        throw InternalErrorGenericError{ "No service available to authenticate user" };
    }

} // namespace lms::api::subsonic
