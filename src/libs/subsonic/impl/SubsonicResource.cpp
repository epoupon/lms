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

#include "services/auth/IPasswordService.hpp"
#include "services/auth/IEnvService.hpp"
#include "services/database/Db.hpp"
#include "services/database/Session.hpp"
#include "services/database/User.hpp"
#include "utils/EnumSet.hpp"
#include "utils/IConfig.hpp"
#include "utils/Logger.hpp"
#include "utils/Service.hpp"
#include "utils/String.hpp"
#include "utils/Utils.hpp"

#include "entrypoints/AlbumSongLists.hpp"
#include "entrypoints/Browsing.hpp"
#include "entrypoints/Bookmarks.hpp"
#include "entrypoints/MediaAnnotation.hpp"
#include "entrypoints/MediaLibraryScanning.hpp"
#include "entrypoints/MediaRetrieval.hpp"
#include "entrypoints/Playlists.hpp"
#include "entrypoints/Searching.hpp"
#include "entrypoints/System.hpp"
#include "entrypoints/UserManagement.hpp"
#include "ParameterParsing.hpp"
#include "ProtocolVersion.hpp"
#include "RequestContext.hpp"
#include "SubsonicId.hpp"
#include "SubsonicResponse.hpp"
#include "Utils.hpp"

using namespace Database;

namespace API::Subsonic
{
    std::unique_ptr<Wt::WResource> createSubsonicResource(Database::Db& db)
    {
        return std::make_unique<SubsonicResource>(db);
    }

    namespace
    {
        std::unordered_map<std::string, ProtocolVersion> readConfigProtocolVersions()
        {
            std::unordered_map<std::string, ProtocolVersion> res;

            Service<IConfig>::get()->visitStrings("api-subsonic-report-old-server-protocol",
                [&](std::string_view client)
                {
                    res.emplace(std::string{ client }, ProtocolVersion{ 1, 12, 0 });
                }, { "DSub" });

            return res;
        }


        std::string parameterMapToDebugString(const Wt::Http::ParameterMap& parameterMap)
        {
            auto censorValue = [](const std::string& type, const std::string& value) -> std::string
                {
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

        void checkUserTypeIsAllowed(RequestContext& context, EnumSet<Database::UserType> allowedUserTypes)
        {
            auto transaction{ context.dbSession.createSharedTransaction() };

            User::pointer currentUser{ User::find(context.dbSession, context.userId) };
            if (!currentUser)
                throw RequestedDataNotFoundError{};

            if (!allowedUserTypes.contains(currentUser->getType()))
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
            RequestHandlerFunc      func;
            EnumSet<UserType>       allowedUserTypes{ UserType::DEMO, UserType::REGULAR, UserType::ADMIN };
            CheckImplementedFunc    checkFunc{};
        };

        static const std::unordered_map<std::string_view, RequestEntryPointInfo> requestEntryPoints
        {
            // System
            {"/ping",                       {handlePingRequest}},
            {"/getLicense",                 {handleGetLicenseRequest}},
            {"/getOpenSubsonicExtensions",  {handleGetOpenSubsonicExtensions}},

            // Browsing
            {"/getMusicFolders",        {handleGetMusicFoldersRequest}},
            {"/getIndexes",             {handleGetIndexesRequest}},
            {"/getMusicDirectory",      {handleGetMusicDirectoryRequest}},
            {"/getGenres",              {handleGetGenresRequest}},
            {"/getArtists",             {handleGetArtistsRequest}},
            {"/getArtist",              {handleGetArtistRequest}},
            {"/getAlbum",               {handleGetAlbumRequest}},
            {"/getSong",                {handleGetSongRequest}},
            {"/getVideos",              {handleNotImplemented}},
            {"/getArtistInfo",          {handleGetArtistInfoRequest}},
            {"/getArtistInfo2",         {handleGetArtistInfo2Request}},
            {"/getAlbumInfo",	        {handleNotImplemented}},
            {"/getAlbumInfo2",          {handleNotImplemented}},
            {"/getSimilarSongs",        {handleGetSimilarSongsRequest}},
            {"/getSimilarSongs2",       {handleGetSimilarSongs2Request}},
            {"/getTopSongs",            {handleNotImplemented}},

            // Album/song lists
            {"/getAlbumList",           {handleGetAlbumListRequest}},
            {"/getAlbumList2",          {handleGetAlbumList2Request}},
            {"/getRandomSongs",         {handleGetRandomSongsRequest}},
            {"/getSongsByGenre",        {handleGetSongsByGenreRequest}},
            {"/getNowPlaying",          {handleNotImplemented}},
            {"/getStarred",             {handleGetStarredRequest}},
            {"/getStarred2",            {handleGetStarred2Request}},

            // Searching
            {"/search",                 {handleNotImplemented}},
            {"/search2",                {handleSearch2Request}},
            {"/search3",                {handleSearch3Request}},

            // Playlists
            {"/getPlaylists",           {handleGetPlaylistsRequest}},
            {"/getPlaylist",            {handleGetPlaylistRequest}},
            {"/createPlaylist",         {handleCreatePlaylistRequest}},
            {"/updatePlaylist",         {handleUpdatePlaylistRequest}},
            {"/deletePlaylist",         {handleDeletePlaylistRequest}},

            // Media retrieval
            {"/hls",                    {handleNotImplemented}},
            {"/getCaptions",            {handleNotImplemented}},
            {"/getLyrics",              {handleNotImplemented}},
            {"/getAvatar",              {handleNotImplemented}},

            // Media annotation
            {"/star",                   {handleStarRequest}},
            {"/unstar",                 {handleUnstarRequest}},
            {"/setRating",              {handleNotImplemented}},
            {"/scrobble",               {handleScrobble}},

            // Sharing
            {"/getShares",              {handleNotImplemented}},
            {"/createShares",           {handleNotImplemented}},
            {"/updateShare",            {handleNotImplemented}},
            {"/deleteShare",            {handleNotImplemented}},

            // Podcast
            {"/getPodcasts",            {handleNotImplemented}},
            {"/getNewestPodcasts",      {handleNotImplemented}},
            {"/refreshPodcasts",        {handleNotImplemented}},
            {"/createPodcastChannel",   {handleNotImplemented}},
            {"/deletePodcastChannel",   {handleNotImplemented}},
            {"/deletePodcastEpisode",   {handleNotImplemented}},
            {"/downloadPodcastEpisode", {handleNotImplemented}},

            // Jukebox
            {"/jukeboxControl",	        {handleNotImplemented}},

            // Internet radio
            {"/getInternetRadioStations",	{handleNotImplemented}},
            {"/createInternetRadioStation",	{handleNotImplemented}},
            {"/updateInternetRadioStation",	{handleNotImplemented}},
            {"/deleteInternetRadioStation",	{handleNotImplemented}},

            // Chat
            {"/getChatMessages",    {handleNotImplemented}},
            {"/addChatMessages",    {handleNotImplemented}},

            // User management
            {"/getUser",            {handleGetUserRequest}},
            {"/getUsers",           {handleGetUsersRequest,     {UserType::ADMIN}}},
            {"/createUser",         {handleCreateUserRequest,   {UserType::ADMIN},                      &Utils::checkSetPasswordImplemented}},
            {"/updateUser",         {handleUpdateUserRequest,   {UserType::ADMIN}}},
            {"/deleteUser",         {handleDeleteUserRequest,   {UserType::ADMIN}}},
            {"/changePassword",     {handleChangePassword,      {UserType::REGULAR, UserType::ADMIN},   &Utils::checkSetPasswordImplemented}},

            // Bookmarks
            {"/getBookmarks",       {handleGetBookmarks}},
            {"/createBookmark",     {handleCreateBookmark}},
            {"/deleteBookmark",     {handleDeleteBookmark}},
            {"/getPlayQueue",       {handleNotImplemented}},
            {"/savePlayQueue",      {handleNotImplemented}},

            // Media library scanning
            {"/getScanStatus",      {Scan::handleGetScanStatus, {UserType::ADMIN}}},
            {"/startScan",          {Scan::handleStartScan,     {UserType::ADMIN}}},
        };

        using MediaRetrievalHandlerFunc = std::function<void(RequestContext&, const Wt::Http::Request&, Wt::Http::Response&)>;
        static std::unordered_map<std::string, MediaRetrievalHandlerFunc> mediaRetrievalHandlers
        {
            // Media retrieval
            {"/download",       handleDownload},
            {"/stream",         handleStream},
            {"/getCoverArt",    handleGetCoverArt},
        };
    }


    SubsonicResource::SubsonicResource(Db& db)
        : _serverProtocolVersionsByClient{ readConfigProtocolVersions() }
        , _db{ db }
    {
    }

    void SubsonicResource::handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response)
    {
        static std::atomic<std::size_t> curRequestId{};

        const std::size_t requestId{ curRequestId++ };

        LMS_LOG(API_SUBSONIC, DEBUG) << "Handling request " << requestId << " '" << request.pathInfo() << "', continuation = " << (request.continuation() ? "true" : "false") << ", params = " << parameterMapToDebugString(request.getParameterMap());

        std::string requestPath{ request.pathInfo() };
        if (StringUtils::stringEndsWith(requestPath, ".view"))
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
                if (itEntryPoint->second.checkFunc)
                    itEntryPoint->second.checkFunc();

                checkUserTypeIsAllowed(requestContext, itEntryPoint->second.allowedUserTypes);

                const Response resp{ (itEntryPoint->second.func)(requestContext) };

                resp.write(response.out(), format);
                response.setMimeType(std::string{ ResponseFormatToMimeType(format) });
                LMS_LOG(API_SUBSONIC, DEBUG) << "Request " << requestId << " '" << requestPath << "' handled!";

                return;
            }

            auto itStreamHandler{ mediaRetrievalHandlers.find(requestPath) };
            if (itStreamHandler != mediaRetrievalHandlers.end())
            {
                itStreamHandler->second(requestContext, request, response);
                LMS_LOG(API_SUBSONIC, DEBUG) << "Request " << requestId << " '" << requestPath << "' handled!";
                return;
            }

            LMS_LOG(API_SUBSONIC, ERROR) << "Unhandled command '" << requestPath << "'";
            throw UnknownEntryPointGenericError{};
        }
        catch (const Error& e)
        {
            LMS_LOG(API_SUBSONIC, ERROR) << "Error while processing request '" << requestPath << "'"
                << ", params = [" << parameterMapToDebugString(request.getParameterMap()) << "]"
                << ", code = " << static_cast<int>(e.getCode()) << ", msg = '" << e.getMessage() << "'";
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

    ClientInfo SubsonicResource::getClientInfo(const Wt::Http::ParameterMap& parameters)
    {
        ClientInfo res;

        if (hasParameter(parameters, "t"))
            throw TokenAuthenticationNotSupportedForLDAPUsersError{};

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
        const ClientInfo clientInfo{ getClientInfo(parameters) };
        const Database::UserId userId{ authenticateUser(request, clientInfo) };

        return { parameters, _db.getTLSSession(), userId, clientInfo, getServerProtocolVersion(clientInfo.name) };
    }

    Database::UserId SubsonicResource::authenticateUser(const Wt::Http::Request& request, const ClientInfo& clientInfo)
    {
        if (auto * authEnvService{ Service<::Auth::IEnvService>::get() })
        {
            const auto checkResult{ authEnvService->processRequest(request) };
            if (checkResult.state != ::Auth::IEnvService::CheckResult::State::Granted)
                throw UserNotAuthorizedError{};

            return *checkResult.userId;
        }
        else if (auto * authPasswordService{ Service<::Auth::IPasswordService>::get() })
        {
            const auto checkResult{ authPasswordService->checkUserPassword(boost::asio::ip::address::from_string(request.clientAddress()), clientInfo.user, clientInfo.password) };

            switch (checkResult.state)
            {
            case Auth::IPasswordService::CheckResult::State::Granted:
                return *checkResult.userId;
                break;
            case Auth::IPasswordService::CheckResult::State::Denied:
                throw WrongUsernameOrPasswordError{};
            case Auth::IPasswordService::CheckResult::State::Throttled:
                throw LoginThrottledGenericError{};
            }
        }

        throw InternalErrorGenericError{ "No service avalaible to authenticate user" };
    }

} // namespace api::subsonic

