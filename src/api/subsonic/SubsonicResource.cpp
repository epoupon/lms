/*
 * Copyright (C) 2019 Emeric Poupon
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

#include <mutex>
#include <numeric>
#include <random>

#include <Wt/Auth/Identity.h>
#include <Wt/WLocalDateTime.h>

#include "av/AvTranscoder.hpp"
#include "cover/CoverArtGrabber.hpp"
#include "database/Artist.hpp"
#include "database/Cluster.hpp"
#include "database/Release.hpp"
#include "database/Track.hpp"
#include "main/Services.hpp"
#include "utils/Logger.hpp"
#include "utils/Utils.hpp"
#include "SubsonicId.hpp"
#include "SubsonicResponse.hpp"

// Requests
#define PING_URL 		"/rest/ping.view"
#define GET_LICENSE_URL		"/rest/getLicense.view"
#define GET_RANDOM_SONGS_URL	"/rest/getRandomSongs.view"
#define GET_ALBUM_LIST_URL	"/rest/getAlbumList.view"
#define GET_ALBUM_LIST2_URL	"/rest/getAlbumList2.view"
#define GET_ALBUM_URL		"/rest/getAlbum.view"
#define GET_ARTIST_URL		"/rest/getArtist.view"
#define GET_ARTISTS_URL		"/rest/getArtists.view"
#define GET_MUSIC_DIRECTORY_URL	"/rest/getMusicDirectory.view"
#define GET_MUSIC_FOLDERS_URL	"/rest/getMusicFolders.view"
#define GET_GENRES_URL		"/rest/getGenres.view"
#define GET_INDEXES_URL		"/rest/getIndexes.view"
#define GET_STARRED_URL		"/rest/getStarred.view"
#define GET_STARRED2_URL	"/rest/getStarred2.view"
#define GET_PLAYLISTS_URL	"/rest/getPlaylists.view"
#define GET_SONGS_BY_GENRE_URL	"/rest/getSongsByGenre.view"

// MediaRetrievals
#define STREAM_URL		"/rest/stream.view"
#define GET_COVER_ART_URL	"/rest/getCoverArt.view"

namespace API::Subsonic
{

// requests
using RequestHandlerFunc = std::function<Response(const Wt::Http::ParameterMap&, Database::Handler&)>;
static Response handlePingRequest(const Wt::Http::ParameterMap& request, Database::Handler& db);
static Response handleGetLicenseRequest(const Wt::Http::ParameterMap& request, Database::Handler& db);
static Response handleGetRandomSongsRequest(const Wt::Http::ParameterMap& request, Database::Handler& db);
static Response handleGetAlbumListRequest(const Wt::Http::ParameterMap& request, Database::Handler& db);
static Response handleGetAlbumList2Request(const Wt::Http::ParameterMap& request, Database::Handler& db);
static Response handleGetAlbumRequest(const Wt::Http::ParameterMap& request, Database::Handler& db);
static Response handleGetArtistRequest(const Wt::Http::ParameterMap& request, Database::Handler& db);
static Response handleGetArtistsRequest(const Wt::Http::ParameterMap& request, Database::Handler& db);
static Response handleGetMusicDirectoryRequest(const Wt::Http::ParameterMap& request, Database::Handler& db);
static Response handleGetMusicFoldersRequest(const Wt::Http::ParameterMap& request, Database::Handler& db);
static Response handleGetGenresRequest(const Wt::Http::ParameterMap& request, Database::Handler& db);
static Response handleGetIndexesRequest(const Wt::Http::ParameterMap& request, Database::Handler& db);
static Response handleGetStarredRequest(const Wt::Http::ParameterMap& request, Database::Handler& db);
static Response handleGetStarred2Request(const Wt::Http::ParameterMap& request, Database::Handler& db);
static Response handleGetPlaylistsRequest(const Wt::Http::ParameterMap& request, Database::Handler& db);
static Response handleGetSongsByGenreRequest(const Wt::Http::ParameterMap& request, Database::Handler& db);

// MediaRetrievals
using MediaRetrivalHandlerFunc = std::function<void(const Wt::Http::Request&, Database::Handler&, Wt::Http::Response& response)>;
void handleStream(const Wt::Http::Request& request, Database::Handler& db, Wt::Http::Response& response);
void handleGetCoverArt(const Wt::Http::Request& request, Database::Handler& db, Wt::Http::Response& response);

static std::map<std::string, RequestHandlerFunc> requestHandlers
{
	{PING_URL,			handlePingRequest},
	{GET_LICENSE_URL,		handleGetLicenseRequest},
	{GET_RANDOM_SONGS_URL,		handleGetRandomSongsRequest},
	{GET_ALBUM_LIST_URL,		handleGetAlbumListRequest},
	{GET_ALBUM_LIST2_URL,		handleGetAlbumList2Request},
	{GET_ALBUM_URL,			handleGetAlbumRequest},
	{GET_ARTIST_URL,		handleGetArtistRequest},
	{GET_ARTISTS_URL,		handleGetArtistsRequest},
	{GET_MUSIC_DIRECTORY_URL,	handleGetMusicDirectoryRequest},
	{GET_MUSIC_FOLDERS_URL,		handleGetMusicFoldersRequest},
	{GET_GENRES_URL,		handleGetGenresRequest},
	{GET_INDEXES_URL,		handleGetIndexesRequest},
	{GET_STARRED_URL,		handleGetStarredRequest},
	{GET_STARRED2_URL,		handleGetStarred2Request},
	{GET_PLAYLISTS_URL,		handleGetPlaylistsRequest},
	{GET_SONGS_BY_GENRE_URL,	handleGetSongsByGenreRequest},
};

static std::map<std::string, MediaRetrivalHandlerFunc> mediaRetrievalHandlers
{
	{STREAM_URL,			handleStream},
	{GET_COVER_ART_URL,		handleGetCoverArt},
};

template<typename T>
boost::optional<T>
getParameterAs(const Wt::Http::ParameterMap& parameterMap, const std::string& param)
{
	boost::optional<T> res;

	auto it = parameterMap.find(param);
	if (it == parameterMap.end())
		return res;

	if (it->second.size() != 1)
		return res;

	return readAs<T>(it->second.front());
}

template<>
boost::optional<std::string>
getParameterAs(const Wt::Http::ParameterMap& parameterMap, const std::string& param)
{
	boost::optional<std::string> res;

	auto it = parameterMap.find(param);
	if (it == parameterMap.end())
		return res;

	if (it->second.size() != 1)
		return res;

	return it->second.front();
}

Id
getParameterAsId(const Wt::Http::ParameterMap& parameterMap, const std::string& param)
{
	auto idParam {getParameterAs<std::string>(parameterMap, "id")};
	if (!idParam)
		throw Error {Error::Code::RequiredParameterMissing};

	auto id {IdFromString(*idParam)};
	if (!id)
		throw Error {"Bad id"};

	return *id;
}


struct ClientInfo
{
	std::string		name;
	ResponseFormat		format;
	std::string		user;
	std::string		password;
};

boost::optional<ClientInfo>
getClientInfo(const Wt::Http::ParameterMap& parameters)
{
	boost::optional<ClientInfo> res {ClientInfo {}};

	// Mandatory parameters
	auto param {getParameterAs<std::string>(parameters, "c")};
	if (!param)
		return {};
	res->name = *param;

	param = getParameterAs<std::string>(parameters, "u");
	if (!param)
		return {};
	res->user = *param;

	param = getParameterAs<std::string>(parameters, "p");
	if (!param)
		return {};

	if (param->find("enc:") == 0)
	{
		param = stringFromHex(param->substr(4));
		if (!param)
			return {};
	}
	res->password = *param;

	// Optional parameters
	param = getParameterAs<std::string>(parameters, "f");
	res->format = (param ? ResponseFormat::json : ResponseFormat::xml); // TODO

	return res;
}

static
bool
checkPassword(Database::Handler& db, const ClientInfo& clientInfo)
{
	auto authUser {db.getUserDatabase().findWithIdentity(Wt::Auth::Identity::LoginName, clientInfo.user)};
	if (!authUser.isValid())
	{
		LMS_LOG(API_SUBSONIC, ERROR) << "Cannot find user '" << clientInfo.user << "'";
		return false;
	}

	return db.getPasswordService().verifyPassword(authUser, clientInfo.password) == Wt::Auth::PasswordResult::PasswordValid;
}

SubsonicResource::SubsonicResource(Wt::Dbo::SqlConnectionPool& connectionPool)
: _db {connectionPool}
{
}

std::vector<std::string>
SubsonicResource::getPaths()
{
	std::vector<std::string> paths;

	for (auto it : requestHandlers)
		paths.emplace_back(it.first);

	for (auto it : mediaRetrievalHandlers)
		paths.emplace_back(it.first);

	return paths;
}

void
SubsonicResource::handleRequest(const Wt::Http::Request &request, Wt::Http::Response &response)
{
	LMS_LOG(API_SUBSONIC, DEBUG) << "REQUEST. Path = '" << request.path() << "', pathInfo = '" << request.pathInfo() << "', queryString = '" << request.queryString() << "'";

	const Wt::Http::ParameterMap parameters {request.getParameterMap()};

	for (const auto it : parameters)
	{
		LMS_LOG(API_SUBSONIC, DEBUG) << "Found param '" << it.first << "'";
		for (const std::string& value : it.second)
			LMS_LOG(API_SUBSONIC, DEBUG) << "\t'" << value << "'";
	}

	auto clientInfo {getClientInfo(parameters)};
	if (!clientInfo)
	{
		LMS_LOG(API_SUBSONIC, ERROR) << "Failed to parse client info";
		return;
	}

	try
	{
		static std::mutex mutex;

		std::unique_lock<std::mutex> lock{mutex}; // For now just handle request s one by one

		if (!checkPassword(_db, *clientInfo))
			throw Error {Error::Code::WrongUsernameOrPassword};

		auto itHandler {requestHandlers.find(request.path())};
		if (itHandler != requestHandlers.end())
		{
			Response resp {(itHandler->second)(request.getParameterMap(), _db)};
			resp.write(response.out(), clientInfo->format);
			response.setMimeType(ResponseFormatToMimeType(clientInfo->format));
			return;
		}

		auto itStreamHandler {mediaRetrievalHandlers.find(request.path())};
		if (itStreamHandler != mediaRetrievalHandlers.end())
		{
			itStreamHandler->second(request, _db, response);
			return;
		}

		LMS_LOG(API_SUBSONIC, ERROR) << "Unhandled command '" << request.path() << "'";

	}
	catch (const Error& e)
	{
		Response resp {Response::createFailedResponse(e)};
		resp.write(response.out(), clientInfo->format);
		response.setMimeType(ResponseFormatToMimeType(clientInfo->format));
	}
}

static
std::string
getArtistNames(const std::vector<Database::Artist::pointer> artists)
{
	if (artists.size() == 1)
		return artists.front()->getName();

	std::vector<std::string> names;
	names.resize(artists.size());

	std::transform(std::cbegin(artists), std::cend(artists), std::begin(names),
			[](const Database::Artist::pointer& artist)
			{
				return artist->getName();
			});

	return joinStrings(names, ", ");
}

static
Response::Node
trackToResponseNode(const Database::Track::pointer& track, bool id3)
{
	Response::Node trackResponse;

	trackResponse.setAttribute("title", track->getName());

	if (!id3)
		trackResponse.setAttribute("isDir", "false");

	trackResponse.setAttribute("id", IdToString({Id::Type::Track, track.id()}));
	trackResponse.setAttribute("coverArt", IdToString({Id::Type::Track, track.id()}));

	auto artists {track->getArtists()};
	if (!artists.empty())
	{
		trackResponse.setAttribute("artist", getArtistNames(artists));

		if (artists.size() == 1 && id3)
			trackResponse.setAttribute("artistId", IdToString({Id::Type::Artist, artists.front().id()}));
	}

	trackResponse.setAttribute("path",  track->getName() + ".mp3");
	trackResponse.setAttribute("bitRate", "128");
	trackResponse.setAttribute("duration", std::to_string(std::chrono::duration_cast<std::chrono::seconds>(track->getDuration()).count()));
	trackResponse.setAttribute("suffix", "mp3");
	trackResponse.setAttribute("contentType", "audio/mpeg");
	trackResponse.setAttribute("size", std::to_string(128000/8 * std::chrono::duration_cast<std::chrono::seconds>(track->getDuration()).count()));

	if (track->getYear())
		trackResponse.setAttribute("year", std::to_string(*track->getYear()));

	if (track->getTrackNumber())
		trackResponse.setAttribute("track", std::to_string(*track->getTrackNumber()));

	if (track->getRelease())
	{
		if (id3)
			trackResponse.setAttribute("albumId", IdToString({Id::Type::Release, track->getRelease().id()}));
		else
			trackResponse.setAttribute("parent", IdToString({Id::Type::Release, track->getRelease().id()}));

		trackResponse.setAttribute("album", track->getRelease()->getName());
	}

	return trackResponse;
}

static
Response::Node
releaseToResponseNode(const Database::Release::pointer& release, bool id3)
{
	Response::Node albumNode;

	if (id3)
	{
		albumNode.setAttribute("name", release->getName());
		albumNode.setAttribute("songCount", std::to_string(release->getTracks().size()));
		albumNode.setAttribute("duration", std::to_string(std::chrono::duration_cast<std::chrono::seconds>(release->getDuration()).count()));
	}
	else
	{
		albumNode.setAttribute("title", release->getName());
		albumNode.setAttribute("isDir", "true");
	}

	albumNode.setAttribute("id", IdToString({Id::Type::Release, release.id()}));
	albumNode.setAttribute("coverArt", IdToString({Id::Type::Release, release.id()}));
	if (release->getReleaseYear())
		albumNode.setAttribute("year", std::to_string(*release->getReleaseYear()));

	auto artists {release->getArtists()};
	if (!artists.empty())
	{
		if (artists.size() > 1)
		{
			albumNode.setAttribute("artist", "Various Artists");

			if (!id3)
				albumNode.setAttribute("parent", IdToString({Id::Type::Root}));
		}
		else
		{
			albumNode.setAttribute("artist", artists.front()->getName());

			if (id3)
				albumNode.setAttribute("artistId", IdToString({Id::Type::Artist, artists.front().id()}));
			else
				albumNode.setAttribute("parent", IdToString({Id::Type::Artist, artists.front().id()}));
		}
	}

	return albumNode;
}

static
Response::Node
artistToResponseNode(const Database::Artist::pointer& artist, bool id3)
{
	Response::Node artistNode;

	artistNode.setAttribute("id", IdToString({Id::Type::Artist, artist.id()}));
	artistNode.setAttribute("name", artist->getName());
	artistNode.setAttribute("albumCount", std::to_string(artist->getReleases().size()));

	if (!id3)
		artistNode.setAttribute("parent", IdToString({Id::Type::Root}));

	return artistNode;
}

static
Response::Node
clusterToResponseNode(const Database::Cluster::pointer& cluster)
{
	Response::Node clusterNode;

	clusterNode.setValue(cluster->getName());
	clusterNode.setAttribute("songCount", std::to_string(cluster->getTrackIds().size()));
	{
		auto releases {Database::Release::getByFilter(*cluster.session(), {cluster.id()})};
		clusterNode.setAttribute("albumCount", std::to_string(releases.size()));
	}

	return clusterNode;
}

// Handlers
Response
handlePingRequest(const Wt::Http::ParameterMap &parameters, Database::Handler& handler)
{
	return Response::createOkResponse();
}

Response
handleGetLicenseRequest(const Wt::Http::ParameterMap &parameters, Database::Handler& handler)
{
	Response response {Response::createOkResponse()};

	Response::Node& licenseNode {response.createNode("license")};
	licenseNode.setAttribute("licenseExpires", "2019-09-03T14:46:43");
	licenseNode.setAttribute("email", "foo@bar.com");
	licenseNode.setAttribute("valid", "true");

	return response;
}

Response
handleGetRandomSongsRequest(const Wt::Http::ParameterMap& parameters, Database::Handler& db)
{
	// Optional params
	auto size {getParameterAs<std::size_t>(parameters, "size")};
	if (!size)
		size = 50;

	*size = std::min(*size, std::size_t {500});

	Wt::Dbo::Transaction transaction {db.getSession()};

	auto tracks {Database::Track::getAllRandom(db.getSession(), *size)};

	LMS_LOG(API_SUBSONIC, DEBUG) << "Got " << tracks.size() << " tracks";

	Response response {Response::createOkResponse()};

	Response::Node& randomSongsNode {response.createNode("randomSongs")};
	for (const Database::Track::pointer& track : tracks)
		randomSongsNode.addArrayChild("song", trackToResponseNode(track, true));

	return response;
}

static
std::vector<Database::Release::pointer> getRandomAlbums(Wt::Dbo::Session& session, std::size_t offset, std::size_t size)
{
	std::vector<Database::Release::pointer> res;

	std::size_t nbReleases {Database::Release::getCount(session)};
	if (offset > nbReleases)
		return res;

	if (offset + size > nbReleases)
		size = nbReleases - offset;

	std::vector<size_t> indexes;
	indexes.resize(nbReleases);
	std::iota(std::begin(indexes), std::end(indexes), 1);

	// As random results are paginated, we need to set a seed for it
	std::random_device r;
	std::seed_seq seed {1337};
	std::mt19937 generator{seed};

	std::shuffle(std::begin(indexes), std::end(indexes), generator);
	std::for_each(std::next(std::begin(indexes), offset), std::next(std::begin(indexes), offset + size),
		[&](std::size_t offset)
		{
			auto release {Database::Release::getAll(session, offset, 1)};
			if (!release.empty())
				res.emplace_back(release.front());
		});

	return res;
}

static
Response
handleGetAlbumListRequestCommon(const Wt::Http::ParameterMap& request, Database::Handler& db, bool id3)
{
	// Mandatory params
	auto type {getParameterAs<std::string>(request, "type")};
	if (!type)
		throw Error {Error::Code::RequiredParameterMissing};

	// Optional params
	auto size {getParameterAs<std::size_t>(request, "size")};
	if (!size)
		size = 10;

	auto offset {getParameterAs<std::size_t>(request, "offset")};
	if (!offset)
		offset = 0;

	std::vector<Database::Release::pointer> releases;

	Wt::Dbo::Transaction transaction {db.getSession()};

	if (*type == "random")
	{
		releases = getRandomAlbums(db.getSession(), *offset, *size);
	}
	else if (*type == "newest")
	{
		auto after {Wt::WLocalDateTime::currentServerDateTime().toUTC().addMonths(-1)};
		releases = Database::Release::getLastAdded(db.getSession(), after, offset, size);
	}
	else if (*type == "alphabeticalByName")
	{
		releases = Database::Release::getAll(db.getSession(), offset, size);
	}
	else
		throw Error {"Unsupported request"};

	LMS_LOG(API_SUBSONIC, DEBUG) << "Got " << releases.size() << " albums";

	Response response {Response::createOkResponse()};
	Response::Node& albumListNode {response.createNode(id3 ? "albumList2" : "albumList")};

	for (const Database::Release::pointer& release : releases)
		albumListNode.addArrayChild("album", releaseToResponseNode(release, id3));

	return response;
}

Response
handleGetAlbumListRequest(const Wt::Http::ParameterMap& request, Database::Handler& db)
{
	return handleGetAlbumListRequestCommon(request, db, false /* no id3 */);
}

Response
handleGetAlbumList2Request(const Wt::Http::ParameterMap& request, Database::Handler& db)
{
	return handleGetAlbumListRequestCommon(request, db, true /* id3 */);
}

Response
handleGetAlbumRequest(const Wt::Http::ParameterMap& request, Database::Handler& db)
{
	// Mandatory params
	Id id {getParameterAsId(request, "id")};

	if (id.type != Id::Type::Release)
		throw Error {"Unexpected id type"};

	Wt::Dbo::Transaction transaction {db.getSession()};

	Database::Release::pointer release {Database::Release::getById(db.getSession(), id.id)};
	if (!release)
		throw Error {Error::Code::RequestedDataNotFound};

	Response response {Response::createOkResponse()};
	Response::Node releaseNode {releaseToResponseNode(release, true /* id3 */)};

	auto tracks {release->getTracks()};
	for (const Database::Track::pointer& track : tracks)
		releaseNode.addArrayChild("song", trackToResponseNode(track, true /* id3 */));

	response.addNode("album", std::move(releaseNode));

	return response;
}

Response
handleGetArtistRequest(const Wt::Http::ParameterMap& request, Database::Handler& db)
{
	// Mandatory params
	Id id {getParameterAsId(request, "id")};

	if (id.type != Id::Type::Artist)
		throw Error {"Unexpected id type"};

	Wt::Dbo::Transaction transaction {db.getSession()};

	Database::Artist::pointer artist {Database::Artist::getById(db.getSession(), id.id)};
	if (!artist)
		throw Error {Error::Code::RequestedDataNotFound};

	Response response {Response::createOkResponse()};
	Response::Node artistNode {artistToResponseNode(artist, true /* id3 */)};

	auto releases {artist->getReleases()};
	for (const Database::Release::pointer& release : releases)
		artistNode.addArrayChild("album", releaseToResponseNode(release, true /* id3 */));

	response.addNode("artist", std::move(artistNode));

	return response;
}

Response
handleGetArtistsRequest(const Wt::Http::ParameterMap& request, Database::Handler& db)
{
	Response response {Response::createOkResponse()};
	Response::Node& artistsNode {response.createNode("artists")};

	Response::Node& indexNode {artistsNode.createArrayChild("index")};
	indexNode.setAttribute("name", "?");

	Wt::Dbo::Transaction transaction {db.getSession()};

	auto artists {Database::Artist::getAll(db.getSession())};
	for (const Database::Artist::pointer& artist : artists)
		indexNode.addArrayChild("artist", artistToResponseNode(artist, true /* id3 */));

	return response;
}


Response
handleGetMusicDirectoryRequest(const Wt::Http::ParameterMap& request, Database::Handler& db)
{
	// Mandatory params
	Id id {getParameterAsId(request, "id")};

	Response response {Response::createOkResponse()};
	Response::Node& directoryNode {response.createNode("directory")};

	switch (id.type)
	{
		case Id::Type::Root:
		{
			Wt::Dbo::Transaction transaction {db.getSession()};

			directoryNode.setAttribute("name", "Music");

			auto artists {Database::Artist::getAll(db.getSession())};
			for (const Database::Artist::pointer& artist : artists)
				directoryNode.addArrayChild("child", artistToResponseNode(artist, false /* no id3 */));

			break;
		}

		case Id::Type::Artist:
		{
			Wt::Dbo::Transaction transaction {db.getSession()};

			auto artist {Database::Artist::getById(db.getSession(), id.id)};
			if (!artist)
				throw Error {Error::Code::RequestedDataNotFound};

			directoryNode.setAttribute("name", artist->getName());

			auto releases {artist->getReleases()};
			for (const Database::Release::pointer& release : releases)
				directoryNode.addArrayChild("child", releaseToResponseNode(release, false /* no id3 */));

			break;
		}

		case Id::Type::Release:
		{
			Wt::Dbo::Transaction transaction {db.getSession()};

			auto release {Database::Release::getById(db.getSession(), id.id)};
			if (!release)
				throw Error {Error::Code::RequestedDataNotFound};

			directoryNode.setAttribute("name", release->getName());

			auto tracks {release->getTracks()};
			for (const Database::Track::pointer& track : tracks)
				directoryNode.addArrayChild("child", trackToResponseNode(track, false));

			break;
		}

		default:
			throw Error {"Unexpected id type"};
	}

	return response;
}

Response
handleGetMusicFoldersRequest(const Wt::Http::ParameterMap& request, Database::Handler& db)
{
	Response response {Response::createOkResponse()};
	Response::Node& musicFoldersNode {response.createNode("musicFolders")};

	Response::Node& musicFolderNode {musicFoldersNode.createArrayChild("musicFolder")};
	musicFolderNode.setAttribute("id", IdToString({Id::Type::Root}));
	musicFolderNode.setAttribute("name", "Music");

	return response;
}

Response
handleGetGenresRequest(const Wt::Http::ParameterMap& request, Database::Handler& db)
{
	Response response {Response::createOkResponse()};

	Response::Node& genresNode {response.createNode("genres")};

	Wt::Dbo::Transaction transaction {db.getSession()};

	auto clusterType {Database::ClusterType::getByName(db.getSession(), "GENRE")};
	if (clusterType)
	{
		auto clusters {clusterType->getClusters()};

		for (const Database::Cluster::pointer& cluster : clusters)
			genresNode.addArrayChild("genre", clusterToResponseNode(cluster));
	}

	return response;
}

Response
handleGetIndexesRequest(const Wt::Http::ParameterMap& request, Database::Handler& db)
{
	Response response {Response::createOkResponse()};
	Response::Node& artistsNode {response.createNode("indexes")};

	Response::Node& indexNode {artistsNode.createArrayChild("index")};
	indexNode.setAttribute("name", "?");

	Wt::Dbo::Transaction transaction {db.getSession()};

	auto artists {Database::Artist::getAll(db.getSession())};
	for (const Database::Artist::pointer& artist : artists)
		indexNode.addArrayChild("artist", artistToResponseNode(artist, false /* no id3 */));

	return response;
}


Response
handleGetStarredRequest(const Wt::Http::ParameterMap& request, Database::Handler& db)
{
	Response response {Response::createOkResponse()};
	response.createNode("starred");

	return response;
}

Response
handleGetStarred2Request(const Wt::Http::ParameterMap& request, Database::Handler& db)
{
	Response response {Response::createOkResponse()};
	response.createNode("starred2");

	return response;
}

Response
handleGetPlaylistsRequest(const Wt::Http::ParameterMap& request, Database::Handler& db)
{
	Response response {Response::createOkResponse()};
	response.createNode("playlists");

	return response;
}

Response
handleGetSongsByGenreRequest(const Wt::Http::ParameterMap& request, Database::Handler& db)
{
	// Mandatory params
	auto genre {getParameterAs<std::string>(request, "genre")};
	if (!genre)
		throw Error {Error::Code::RequiredParameterMissing};

	// Optional params
	auto size {getParameterAs<std::size_t>(request, "count")};
	if (!size)
		size = 10;

	*size = std::min(*size, std::size_t {500});

	auto offset {getParameterAs<std::size_t>(request, "offset")};
	if (!offset)
		offset = 0;

	LMS_LOG(API_SUBSONIC, DEBUG) << "genre ='" << *genre << "'";

	Wt::Dbo::Transaction transaction {db.getSession()};

	auto clusterType {Database::ClusterType::getByName(db.getSession(), "GENRE")};
	if (!clusterType)
		throw Error {Error::Code::RequestedDataNotFound};

	auto cluster {clusterType->getCluster(*genre)};
	if (!cluster)
		throw Error {Error::Code::RequestedDataNotFound};

	Response response {Response::createOkResponse()};
	Response::Node& songsByGenreNode {response.createNode("songsByGenre")};

	bool more;
	auto tracks {Database::Track::getByFilter(db.getSession(), {cluster.id()}, {}, offset, size, more)};
	for (const Database::Track::pointer& track : tracks)
		songsByGenreNode.addArrayChild("song", trackToResponseNode(track, true));

	return response;
}

static
std::shared_ptr<Av::Transcoder>
createTranscoder(const Wt::Http::ParameterMap& request, Database::Handler& db)
{
	// Mandatory params
	Id id {getParameterAsId(request, "id")};

	// Optional params
	auto maxBitRate {getParameterAs<std::size_t>(request, "maxBitRate")};
	if (!maxBitRate)
		maxBitRate = 128;

	*maxBitRate = clamp(*maxBitRate, std::size_t {48}, std::size_t {320});

	boost::filesystem::path trackPath;
	{
		Wt::Dbo::Transaction transaction {db.getSession()};

		auto track {Database::Track::getById(db.getSession(), id.id)};
		if (!track)
		{
			LMS_LOG(API_SUBSONIC, ERROR) << "Bad track id";
			throw Error {Error::Code::RequestedDataNotFound};
		}

		trackPath = track->getPath();
	}

	Av::TranscodeParameters parameters {};

	parameters.bitrate = *maxBitRate * 1000;
	parameters.encoding = Av::Encoding::MP3;

	return std::make_shared<Av::Transcoder>(trackPath, parameters);
}

void
handleStream(const Wt::Http::Request& request, Database::Handler& db, Wt::Http::Response& response)
{
	LMS_LOG(API_SUBSONIC, DEBUG) << "STREAM";

	// TODO store only weak ptrs and use a ring container to store shared_ptr?
	std::shared_ptr<Av::Transcoder> transcoder;

	Wt::Http::ResponseContinuation* continuation {request.continuation()};
	if (!continuation)
	{
		transcoder = createTranscoder(request.getParameterMap(), db);
		response.setMimeType(Av::encodingToMimetype(Av::Encoding::MP3));
		transcoder->start();
	}
	else
	{
		LMS_LOG(UI, DEBUG) << "Continuation!";
		transcoder = Wt::cpp17::any_cast<std::shared_ptr<Av::Transcoder>>(continuation->data());
	}

	if (!transcoder)
		throw Error {"transcoding failed"};

	if (!transcoder->isComplete())
	{
		static constexpr std::size_t chunkSize {65536*4};

		std::vector<unsigned char> data;
		data.reserve(chunkSize);

		transcoder->process(data, chunkSize);

		LMS_LOG(API_SUBSONIC, DEBUG) << "Writing " << data.size() << " bytes...";
		response.out().write(reinterpret_cast<char*>(&data[0]), data.size());

		if (!response.out())
		{
			LMS_LOG(UI, ERROR) << "Write failed!";
			return;
		}
	}

	if (!transcoder->isComplete())
	{
		continuation = response.createContinuation();
		continuation->setData(transcoder);
	}
	else
		LMS_LOG(API_SUBSONIC, DEBUG) << "No more data!";
}

void
handleGetCoverArt(const Wt::Http::Request& request, Database::Handler& db, Wt::Http::Response& response)
{
	LMS_LOG(API_SUBSONIC, DEBUG) << "STREAM";

	// Mandatory params
	Id id {getParameterAsId(request.getParameterMap(), "id")};

	auto size {getParameterAs<std::size_t>(request.getParameterMap(), "size")};
	if (!size)
		size = 256;

	*size = clamp(*size, std::size_t {32}, std::size_t {1024});

	std::vector<uint8_t> cover;
	switch (id.type)
	{
		case Id::Type::Track:
			cover = getServices().coverArtGrabber->getFromTrack(db.getSession(), id.id, Image::Format::JPEG, *size);
			break;
		case Id::Type::Release:
			cover = getServices().coverArtGrabber->getFromRelease(db.getSession(), id.id, Image::Format::JPEG, *size);
			break;
		default:
			throw Error {"Unexpected id type"};
	}

	response.setMimeType( Image::format_to_mimeType(Image::Format::JPEG) );
	response.out().write(reinterpret_cast<const char *>(&cover[0]), cover.size());
}

} // namespace api::subsonic

