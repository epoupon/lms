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
#include "database/TrackList.hpp"
#include "main/Services.hpp"
#include "similarity/SimilaritySearcher.hpp"
#include "utils/Logger.hpp"
#include "utils/Utils.hpp"
#include "SubsonicId.hpp"
#include "SubsonicResponse.hpp"

#define CLUSTER_TYPE_GENRE	"GENRE"
#define REPORTED_BITRATE	128

// Requests
#define PING_URL 		"/rest/ping.view"
#define CREATE_PLAYLIST_URL	"/rest/createPlaylist.view"
#define DELETE_PLAYLIST_URL	"/rest/deletePlaylist.view"
#define GET_LICENSE_URL		"/rest/getLicense.view"
#define GET_RANDOM_SONGS_URL	"/rest/getRandomSongs.view"
#define GET_ALBUM_LIST_URL	"/rest/getAlbumList.view"
#define GET_ALBUM_LIST2_URL	"/rest/getAlbumList2.view"
#define GET_ALBUM_URL		"/rest/getAlbum.view"
#define GET_ARTIST_URL		"/rest/getArtist.view"
#define GET_ARTIST_INFO_URL	"/rest/getArtistInfo.view"
#define GET_ARTIST_INFO2_URL	"/rest/getArtistInfo2.view"
#define GET_ARTISTS_URL		"/rest/getArtists.view"
#define GET_MUSIC_DIRECTORY_URL	"/rest/getMusicDirectory.view"
#define GET_MUSIC_FOLDERS_URL	"/rest/getMusicFolders.view"
#define GET_GENRES_URL		"/rest/getGenres.view"
#define GET_INDEXES_URL		"/rest/getIndexes.view"
#define GET_STARRED_URL		"/rest/getStarred.view"
#define GET_STARRED2_URL	"/rest/getStarred2.view"
#define GET_PLAYLIST_URL	"/rest/getPlaylist.view"
#define GET_PLAYLISTS_URL	"/rest/getPlaylists.view"
#define GET_SONGS_BY_GENRE_URL	"/rest/getSongsByGenre.view"
#define SEARCH2_URL		"/rest/search2.view"
#define SEARCH3_URL		"/rest/search3.view"
#define UPDATE_PLAYLIST_URL	"/rest/updatePlaylist.view"

// MediaRetrievals
#define STREAM_URL		"/rest/stream.view"
#define GET_COVER_ART_URL	"/rest/getCoverArt.view"

template<>
boost::optional<API::Subsonic::Id>
readAs(const std::string& str)
{
	return API::Subsonic::IdFromString(str);
}

template<>
boost::optional<bool>
readAs(const std::string& str)
{
	if (str == "true")
		return true;
	else if (str == "false")
		return false;

	return {};
}

namespace API::Subsonic
{
struct ClientInfo
{
	std::string name;
	std::string user;
	std::string password;
};

struct RequestContext
{
	const Wt::Http::ParameterMap& parameters;
	Database::Handler& db;
	std::string userName;
};

// requests
using RequestHandlerFunc = std::function<Response(RequestContext& context)>;
static Response handlePingRequest(RequestContext& context);
static Response handleCreatePlaylistRequest(RequestContext& context);
static Response handleDeletePlaylistRequest(RequestContext& context);
static Response handleGetLicenseRequest(RequestContext& context);
static Response handleGetRandomSongsRequest(RequestContext& context);
static Response handleGetAlbumListRequest(RequestContext& context);
static Response handleGetAlbumList2Request(RequestContext& context);
static Response handleGetAlbumRequest(RequestContext& context);
static Response handleGetArtistRequest(RequestContext& context);
static Response handleGetArtistInfoRequest(RequestContext& context);
static Response handleGetArtistInfo2Request(RequestContext& context);
static Response handleGetArtistsRequest(RequestContext& context);
static Response handleGetMusicDirectoryRequest(RequestContext& context);
static Response handleGetMusicFoldersRequest(RequestContext& context);
static Response handleGetGenresRequest(RequestContext& context);
static Response handleGetIndexesRequest(RequestContext& context);
static Response handleGetStarredRequest(RequestContext& context);
static Response handleGetStarred2Request(RequestContext& context);
static Response handleGetPlaylistRequest(RequestContext& context);
static Response handleGetPlaylistsRequest(RequestContext& context);
static Response handleGetSongsByGenreRequest(RequestContext& context);
static Response handleSearch2Request(RequestContext& context);
static Response handleSearch3Request(RequestContext& context);
static Response handleUpdatePlaylistRequest(RequestContext& context);

// MediaRetrievals
using MediaRetrievalHandlerFunc = std::function<void(RequestContext&, Wt::Http::ResponseContinuation*, Wt::Http::Response& response)>;
void handleStream(RequestContext& context, Wt::Http::ResponseContinuation* continuation, Wt::Http::Response& response);
void handleGetCoverArt(RequestContext& context, Wt::Http::ResponseContinuation* continuation, Wt::Http::Response& response);

static std::map<std::string, RequestHandlerFunc> requestHandlers
{
	{PING_URL,			handlePingRequest},
	{CREATE_PLAYLIST_URL,		handleCreatePlaylistRequest},
	{DELETE_PLAYLIST_URL,		handleDeletePlaylistRequest},
	{GET_LICENSE_URL,		handleGetLicenseRequest},
	{GET_RANDOM_SONGS_URL,		handleGetRandomSongsRequest},
	{GET_ALBUM_LIST_URL,		handleGetAlbumListRequest},
	{GET_ALBUM_LIST2_URL,		handleGetAlbumList2Request},
	{GET_ALBUM_URL,			handleGetAlbumRequest},
	{GET_ARTIST_URL,		handleGetArtistRequest},
	{GET_ARTIST_INFO_URL,		handleGetArtistInfoRequest},
	{GET_ARTIST_INFO2_URL,		handleGetArtistInfo2Request},
	{GET_ARTISTS_URL,		handleGetArtistsRequest},
	{GET_MUSIC_DIRECTORY_URL,	handleGetMusicDirectoryRequest},
	{GET_MUSIC_FOLDERS_URL,		handleGetMusicFoldersRequest},
	{GET_GENRES_URL,		handleGetGenresRequest},
	{GET_INDEXES_URL,		handleGetIndexesRequest},
	{GET_STARRED_URL,		handleGetStarredRequest},
	{GET_STARRED2_URL,		handleGetStarred2Request},
	{GET_PLAYLIST_URL,		handleGetPlaylistRequest},
	{GET_PLAYLISTS_URL,		handleGetPlaylistsRequest},
	{GET_SONGS_BY_GENRE_URL,	handleGetSongsByGenreRequest},
	{SEARCH2_URL,			handleSearch2Request},
	{SEARCH3_URL,			handleSearch3Request},
	{UPDATE_PLAYLIST_URL,		handleUpdatePlaylistRequest},
};

static std::map<std::string, MediaRetrievalHandlerFunc> mediaRetrievalHandlers
{
	{STREAM_URL,			handleStream},
	{GET_COVER_ART_URL,		handleGetCoverArt},
};

static
std::string
makeNameFilesystemCompatible(const std::string& name)
{
	return replaceInString(name, "/", "_");
}

template<typename T>
std::vector<T>
getMultiParametersAs(const Wt::Http::ParameterMap& parameterMap, const std::string& param)
{
	std::vector<T> res;

	auto it = parameterMap.find(param);
	if (it == parameterMap.end())
		return res;

	for (const std::string& param : it->second)
	{
		auto val {readAs<T>(param)};
		if (!val)
		{
			res.clear();
			return res;
		}

		res.emplace_back(std::move(*val));
	}

	return res;
}

template<typename T>
std::vector<T>
getMandatoryMultiParametersAs(const Wt::Http::ParameterMap& parameterMap, const std::string& param)
{
	std::vector<T> res {getMultiParametersAs<T>(parameterMap, param)};
	if (res.empty())
		throw Error {Error::Code::RequiredParameterMissing};

	return res;
}

template<typename T>
boost::optional<T>
getParameterAs(const Wt::Http::ParameterMap& parameterMap, const std::string& param)
{
	std::vector<T> params {getMultiParametersAs<T>(parameterMap, param)};

	if (params.size() != 1)
		return {};

	return T { std::move(params.front()) };
}

template<typename T>
T
getMandatoryParameterAs(const Wt::Http::ParameterMap& parameterMap, const std::string& param)
{
	auto res {getParameterAs<T>(parameterMap, param)};
	if (!res)
		throw Error {Error::Code::RequiredParameterMissing};

	return *res;
}

ClientInfo
getClientInfo(const Wt::Http::ParameterMap& parameters)
{
	ClientInfo res;

	// Mandatory parameters
	res.name = getMandatoryParameterAs<std::string>(parameters, "c");
	res.user = getMandatoryParameterAs<std::string>(parameters, "u");

	{
		std::string password {getMandatoryParameterAs<std::string>(parameters, "p")};
		if (password.find("enc:") == 0)
		{
			auto decodedPassword {stringFromHex(password.substr(4))};
			if (!decodedPassword)
				throw Error {Error::Code::WrongUsernameOrPassword};

			res.password = *decodedPassword;
		}
		else
			res.password = password;
	}

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

static
std::string parameterMapToDebugString(const Wt::Http::ParameterMap& parameterMap)
{
	auto censorValue = [](const std::string& type, const std::string& value)
	{
		if (type == "p")
			return std::string {"CENSORED"};
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

void
SubsonicResource::handleRequest(const Wt::Http::Request &request, Wt::Http::Response &response)
{
	LMS_LOG(API_SUBSONIC, DEBUG) << "Handling request '" << request.path() << "', params = " << parameterMapToDebugString(request.getParameterMap());

	const Wt::Http::ParameterMap& parameters {request.getParameterMap()};

	// Optional parameters
	ResponseFormat format {getParameterAs<std::string>(parameters, "f").get_value_or("xml") == "json" ? ResponseFormat::json : ResponseFormat::xml};

	try
	{
		static std::mutex mutex;

		ClientInfo clientInfo {getClientInfo(parameters)};

		std::unique_lock<std::mutex> lock{mutex}; // For now just handle request s one by one

		if (!checkPassword(_db, clientInfo))
			throw Error {Error::Code::WrongUsernameOrPassword};

		RequestContext requestContext {.parameters = parameters, .db = _db, .userName = clientInfo.user};

		auto itHandler {requestHandlers.find(request.path())};
		if (itHandler != requestHandlers.end())
		{
			Response resp {(itHandler->second)(requestContext)};
			resp.write(response.out(), format);
			response.setMimeType(ResponseFormatToMimeType(format));
			return;
		}

		auto itStreamHandler {mediaRetrievalHandlers.find(request.path())};
		if (itStreamHandler != mediaRetrievalHandlers.end())
		{
			itStreamHandler->second(requestContext, request.continuation(), response);
			return;
		}

		LMS_LOG(API_SUBSONIC, ERROR) << "Unhandled command '" << request.path() << "'";

	}
	catch (const Error& e)
	{
		LMS_LOG(API_SUBSONIC, ERROR) << "Error while processing command. code = " << static_cast<int>(e.getCode()) << ", msg = '" << e.getMessage() << "'";
		Response resp {Response::createFailedResponse(e)};
		resp.write(response.out(), format);
		response.setMimeType(ResponseFormatToMimeType(format));
	}
}

static
std::string
getArtistNames(const std::vector<Database::Artist::pointer>& artists)
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
std::string
getTrackPath(const Database::Track::pointer& track)
{
	std::string path;

	// TODO encode '/'?

	if (track->getRelease())
	{
		auto artists {track->getRelease()->getArtists()};

		if (artists.size() > 1)
			path = "Various Artists/";
		else if (artists.size() == 1)
			path = makeNameFilesystemCompatible(artists.front()->getName()) + "/";

		path += makeNameFilesystemCompatible(track->getRelease()->getName()) + "/";
	}

	if (track->getDiscNumber())
		path += *track->getDiscNumber() + "-";
	if (track->getTrackNumber())
		path += *track->getTrackNumber() + "-";

	return path + makeNameFilesystemCompatible(track->getName()) + ".mp3";
}

static
Response::Node
trackToResponseNode(const Database::Track::pointer& track)
{
	Response::Node trackResponse;

	trackResponse.setAttribute("id", IdToString({Id::Type::Track, track.id()}));
	trackResponse.setAttribute("isDir", "false");
	trackResponse.setAttribute("title", track->getName());
	if (track->getTrackNumber())
		trackResponse.setAttribute("track", std::to_string(*track->getTrackNumber()));
	if (track->getDiscNumber())
		trackResponse.setAttribute("discNumber", std::to_string(*track->getDiscNumber()));
	if (track->getYear())
		trackResponse.setAttribute("year", std::to_string(*track->getYear()));
	trackResponse.setAttribute("size", std::to_string(REPORTED_BITRATE*1000/8 * std::chrono::duration_cast<std::chrono::seconds>(track->getDuration()).count()));

	trackResponse.setAttribute("coverArt", IdToString({Id::Type::Track, track.id()}));

	auto artists {track->getArtists()};
	if (!artists.empty())
	{
		trackResponse.setAttribute("artist", getArtistNames(artists));

		if (artists.size() == 1)
			trackResponse.setAttribute("artistId", IdToString({Id::Type::Artist, artists.front().id()}));
	}

	if (track->getRelease())
	{
		trackResponse.setAttribute("album", track->getRelease()->getName());
		trackResponse.setAttribute("albumId", IdToString({Id::Type::Release, track->getRelease().id()}));
		trackResponse.setAttribute("parent", IdToString({Id::Type::Release, track->getRelease().id()}));
	}

	trackResponse.setAttribute("path", getTrackPath(track));
	trackResponse.setAttribute("bitRate", std::to_string(REPORTED_BITRATE));
	trackResponse.setAttribute("duration", std::to_string(std::chrono::duration_cast<std::chrono::seconds>(track->getDuration()).count()));
	trackResponse.setAttribute("suffix", "mp3");
	trackResponse.setAttribute("contentType", "audio/mpeg");
	trackResponse.setAttribute("type", "music");

	// Report the first GENRE for this track
	Database::ClusterType::pointer clusterType {Database::ClusterType::getByName(*track.session(), CLUSTER_TYPE_GENRE)};
	if (clusterType)
	{
		auto clusters {track->getClusterGroups({clusterType}, 1)};
		if (!clusters.empty() && !clusters.front().empty())
			trackResponse.setAttribute("genre", clusters.front().front()->getName());
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
	if (artists.empty() && !id3)
	{
		albumNode.setAttribute("parent", IdToString({Id::Type::Root}));
	}
	else if (!artists.empty())
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

	if (id3)
	{
		// Report the first GENRE for this track
		Database::ClusterType::pointer clusterType {Database::ClusterType::getByName(*release.session(), CLUSTER_TYPE_GENRE)};
		if (clusterType)
		{
			auto clusters {release->getClusterGroups({clusterType}, 1)};
			if (!clusters.empty() && !clusters.front().empty())
				albumNode.setAttribute("genre", clusters.front().front()->getName());
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

	if (id3)
		artistNode.setAttribute("albumCount", std::to_string(artist->getReleases().size()));

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
handlePingRequest(RequestContext& context)
{
	return Response::createOkResponse();
}

Response
handleCreatePlaylistRequest(RequestContext& context)
{
	// Optional params
	auto id {getParameterAs<Id>(context.parameters, "playlistId")};
	if (id && id->type != Id::Type::Playlist)
		throw Error {Error::CustomType::BadId};

	auto name {getParameterAs<std::string>(context.parameters, "name")};

	std::vector<Id> trackIds {getMultiParametersAs<Id>(context.parameters, "songId")};
	if (!std::all_of(std::cbegin(trackIds), std::cend(trackIds ), [](const Id& id) { return id.type == Id::Type::Track; }))
		throw Error {Error::CustomType::BadId};

	if (!name && !id)
		throw Error {Error::Code::RequiredParameterMissing};

	Wt::Dbo::Transaction transaction {context.db.getSession()};

	Database::User::pointer user {context.db.getUser(context.userName)};
	if (!user)
		throw Error {Error::Code::RequestedDataNotFound};

	Database::TrackList::pointer tracklist;
	if (id)
	{
		tracklist = Database::TrackList::getById(context.db.getSession(), id->value);
		if (!tracklist
			|| tracklist->getUser() != user
			|| tracklist->getType() != Database::TrackList::Type::Playlist)
		{
			throw Error {Error::Code::RequestedDataNotFound};
		}

		if (name)
			tracklist.modify()->setName(*name);
	}
	else
	{
		tracklist = Database::TrackList::create(context.db.getSession(), *name, Database::TrackList::Type::Playlist, false, user);
	}

	for (const Id& trackId : trackIds)
	{
		Database::Track::pointer track {Database::Track::getById(context.db.getSession(), trackId.value)};
		if (!track)
			continue;

		Database::TrackListEntry::create(context.db.getSession(), track, tracklist );
	}

	return Response::createOkResponse();
}

Response
handleDeletePlaylistRequest(RequestContext& context)
{
	// Optional params
	Id id {getMandatoryParameterAs<Id>(context.parameters, "id")};
	if (id.type != Id::Type::Playlist)
		throw Error {Error::CustomType::BadId};

	Wt::Dbo::Transaction transaction {context.db.getSession()};

	Database::User::pointer user {context.db.getUser(context.userName)};
	if (!user)
		throw Error {Error::Code::RequestedDataNotFound};

	Database::TrackList::pointer tracklist {Database::TrackList::getById(context.db.getSession(), id.value)};
	if (!tracklist
		|| tracklist->getUser() != user
		|| tracklist->getType() != Database::TrackList::Type::Playlist)
	{
		throw Error {Error::Code::RequestedDataNotFound};
	}

	tracklist.remove();

	return Response::createOkResponse();
}

Response
handleGetLicenseRequest(RequestContext& context)
{
	Response response {Response::createOkResponse()};

	Response::Node& licenseNode {response.createNode("license")};
	licenseNode.setAttribute("licenseExpires", "2019-09-03T14:46:43");
	licenseNode.setAttribute("email", "foo@bar.com");
	licenseNode.setAttribute("valid", "true");

	return response;
}

Response
handleGetRandomSongsRequest(RequestContext& context)
{
	// Optional params
	std::size_t size {getParameterAs<std::size_t>(context.parameters, "size").get_value_or(50)};
	size = std::min(size, std::size_t {500});

	Wt::Dbo::Transaction transaction {context.db.getSession()};

	auto tracks {Database::Track::getAllRandom(context.db.getSession(), size)};

	Response response {Response::createOkResponse()};

	Response::Node& randomSongsNode {response.createNode("randomSongs")};
	for (const Database::Track::pointer& track : tracks)
		randomSongsNode.addArrayChild("song", trackToResponseNode(track));

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
handleGetAlbumListRequestCommon(const RequestContext& context, bool id3)
{
	// Mandatory params
	std::string type {getMandatoryParameterAs<std::string>(context.parameters, "type")};

	// Optional params
	std::size_t size {getParameterAs<std::size_t>(context.parameters, "size").get_value_or(10)};
	std::size_t offset {getParameterAs<std::size_t>(context.parameters, "offset").get_value_or(0)};

	std::vector<Database::Release::pointer> releases;

	Wt::Dbo::Transaction transaction {context.db.getSession()};

	if (type == "random")
	{
		releases = getRandomAlbums(context.db.getSession(), offset, size);
	}
	else if (type == "newest")
	{
		auto after {Wt::WLocalDateTime::currentServerDateTime().toUTC().addMonths(-6)};
		releases = Database::Release::getLastAdded(context.db.getSession(), after, offset, size);
	}
	else if (type == "alphabeticalByName")
	{
		releases = Database::Release::getAll(context.db.getSession(), offset, size);
	}
	else if (type == "byGenre")
	{
		// Mandatory param
		std::string genre {getMandatoryParameterAs<std::string>(context.parameters, "genre")};

		Database::ClusterType::pointer clusterType {Database::ClusterType::getByName(context.db.getSession(), CLUSTER_TYPE_GENRE)};
		if (clusterType)
		{
			Database::Cluster::pointer cluster {clusterType->getCluster(genre)};
			if (cluster)
			{
				bool more;
				releases = Database::Release::getByFilter(context.db.getSession(), {cluster.id()}, {}, offset, size, more);
			}
		}
	}
	else
		throw Error {Error::CustomType::NotImplemented};

	Response response {Response::createOkResponse()};
	Response::Node& albumListNode {response.createNode(id3 ? "albumList2" : "albumList")};

	for (const Database::Release::pointer& release : releases)
		albumListNode.addArrayChild("album", releaseToResponseNode(release, id3));

	return response;
}

Response
handleGetAlbumListRequest(RequestContext& context)
{
	return handleGetAlbumListRequestCommon(context, false /* no id3 */);
}

Response
handleGetAlbumList2Request(RequestContext& context)
{
	return handleGetAlbumListRequestCommon(context, true /* id3 */);
}

Response
handleGetAlbumRequest(RequestContext& context)
{
	// Mandatory params
	Id id {getMandatoryParameterAs<Id>(context.parameters, "id")};

	if (id.type != Id::Type::Release)
		throw Error {Error::CustomType::BadId};

	Wt::Dbo::Transaction transaction {context.db.getSession()};

	Database::Release::pointer release {Database::Release::getById(context.db.getSession(), id.value)};
	if (!release)
		throw Error {Error::Code::RequestedDataNotFound};

	Response response {Response::createOkResponse()};
	Response::Node releaseNode {releaseToResponseNode(release, true /* id3 */)};

	auto tracks {release->getTracks()};
	for (const Database::Track::pointer& track : tracks)
		releaseNode.addArrayChild("song", trackToResponseNode(track));

	response.addNode("album", std::move(releaseNode));

	return response;
}

Response
handleGetArtistRequest(RequestContext& context)
{
	// Mandatory params
	Id id {getMandatoryParameterAs<Id>(context.parameters, "id")};

	if (id.type != Id::Type::Artist)
		throw Error {Error::CustomType::BadId};

	Wt::Dbo::Transaction transaction {context.db.getSession()};

	Database::Artist::pointer artist {Database::Artist::getById(context.db.getSession(), id.value)};
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

static

Response handleGetArtistInfoRequestCommon(RequestContext& context, bool id3)
{
	// Mandatory params
	Id id {getMandatoryParameterAs<Id>(context.parameters, "id")};
	if (id.type != Id::Type::Artist)
		throw Error {Error::CustomType::BadId};

	// Optional params
	std::size_t count {getParameterAs<std::size_t>(context.parameters, "count").get_value_or(10)};

	Wt::Dbo::Transaction transaction {context.db.getSession()};

	Database::Artist::pointer artist {Database::Artist::getById(context.db.getSession(), id.value)};
	if (!artist)
		throw Error {Error::Code::RequestedDataNotFound};

	Response response {Response::createOkResponse()};
	Response::Node& artistInfoNode {response.createNode(id3 ? "artistInfo2" : "artistInfo")};

	if (!artist->getMBID().empty())
		artistInfoNode.createChild("musicBrainzId").setValue(artist->getMBID());

	auto similarArtistsId {getServices().similaritySearcher->getSimilarArtists(context.db.getSession(), artist.id(), count)};
	for ( const auto& similarArtistId : similarArtistsId )
	{
		Database::Artist::pointer similarArtist {Database::Artist::getById(context.db.getSession(), similarArtistId)};

		if (similarArtist)
			artistInfoNode.addArrayChild("similarArtist", artistToResponseNode(similarArtist, id3));
	}

	return response;
}

Response handleGetArtistInfoRequest(RequestContext& context)
{
	return handleGetArtistInfoRequestCommon(context, false /* no id3 */);
}
static Response handleGetArtistInfo2Request(RequestContext& context)
{
	return handleGetArtistInfoRequestCommon(context, true /* id3 */);
}

Response
handleGetArtistsRequest(RequestContext& context)
{
	Response response {Response::createOkResponse()};
	Response::Node& artistsNode {response.createNode("artists")};

	Response::Node& indexNode {artistsNode.createArrayChild("index")};
	indexNode.setAttribute("name", "?");

	Wt::Dbo::Transaction transaction {context.db.getSession()};

	auto artists {Database::Artist::getAll(context.db.getSession())};
	for (const Database::Artist::pointer& artist : artists)
		indexNode.addArrayChild("artist", artistToResponseNode(artist, true /* id3 */));

	return response;
}


Response
handleGetMusicDirectoryRequest(RequestContext& context)
{
	// Mandatory params
	Id id {getMandatoryParameterAs<Id>(context.parameters, "id")};

	Response response {Response::createOkResponse()};
	Response::Node& directoryNode {response.createNode("directory")};

	directoryNode.setAttribute("id", IdToString(id));

	switch (id.type)
	{
		case Id::Type::Root:
		{
			Wt::Dbo::Transaction transaction {context.db.getSession()};

			directoryNode.setAttribute("name", "Music");

			auto artists {Database::Artist::getAll(context.db.getSession())};
			for (const Database::Artist::pointer& artist : artists)
				directoryNode.addArrayChild("child", artistToResponseNode(artist, false /* no id3 */));

			break;
		}

		case Id::Type::Artist:
		{
			Wt::Dbo::Transaction transaction {context.db.getSession()};

			auto artist {Database::Artist::getById(context.db.getSession(), id.value)};
			if (!artist)
				throw Error {Error::Code::RequestedDataNotFound};

			directoryNode.setAttribute("name", makeNameFilesystemCompatible(artist->getName()));

			auto releases {artist->getReleases()};
			for (const Database::Release::pointer& release : releases)
				directoryNode.addArrayChild("child", releaseToResponseNode(release, false /* no id3 */));

			break;
		}

		case Id::Type::Release:
		{
			Wt::Dbo::Transaction transaction {context.db.getSession()};

			auto release {Database::Release::getById(context.db.getSession(), id.value)};
			if (!release)
				throw Error {Error::Code::RequestedDataNotFound};

			directoryNode.setAttribute("name", makeNameFilesystemCompatible(release->getName()));

			auto tracks {release->getTracks()};
			for (const Database::Track::pointer& track : tracks)
				directoryNode.addArrayChild("child", trackToResponseNode(track));

			break;
		}

		default:
			throw Error {Error::CustomType::BadId};
	}

	return response;
}

Response
handleGetMusicFoldersRequest(RequestContext& context)
{
	Response response {Response::createOkResponse()};
	Response::Node& musicFoldersNode {response.createNode("musicFolders")};

	Response::Node& musicFolderNode {musicFoldersNode.createArrayChild("musicFolder")};
	musicFolderNode.setAttribute("id", IdToString({Id::Type::Root}));
	musicFolderNode.setAttribute("name", "Music");

	return response;
}

Response
handleGetGenresRequest(RequestContext& context)
{
	Response response {Response::createOkResponse()};

	Response::Node& genresNode {response.createNode("genres")};

	Wt::Dbo::Transaction transaction {context.db.getSession()};

	auto clusterType {Database::ClusterType::getByName(context.db.getSession(), CLUSTER_TYPE_GENRE)};
	if (clusterType)
	{
		auto clusters {clusterType->getClusters()};

		for (const Database::Cluster::pointer& cluster : clusters)
			genresNode.addArrayChild("genre", clusterToResponseNode(cluster));
	}

	return response;
}

Response
handleGetIndexesRequest(RequestContext& context)
{
	Response response {Response::createOkResponse()};
	Response::Node& artistsNode {response.createNode("indexes")};

	Response::Node& indexNode {artistsNode.createArrayChild("index")};
	indexNode.setAttribute("name", "?");

	Wt::Dbo::Transaction transaction {context.db.getSession()};

	auto artists {Database::Artist::getAll(context.db.getSession())};
	for (const Database::Artist::pointer& artist : artists)
		indexNode.addArrayChild("artist", artistToResponseNode(artist, false /* no id3 */));

	return response;
}


Response
handleGetStarredRequest(RequestContext& context)
{
	throw Error{Error::CustomType::NotImplemented};
}

Response
handleGetStarred2Request(RequestContext& context)
{
	throw Error{Error::CustomType::NotImplemented};
}

Response::Node
tracklistToResponseNode(const Database::TrackList::pointer& tracklist, Database::Handler& db)
{
	Response::Node playlistNode;

	playlistNode.setAttribute("id", IdToString({Id::Type::Playlist, tracklist.id()}));
	playlistNode.setAttribute("name", tracklist->getName());
	playlistNode.setAttribute("songCount", std::to_string(tracklist->getCount()));
	playlistNode.setAttribute("duration", std::to_string(std::chrono::duration_cast<std::chrono::seconds>(tracklist->getDuration()).count()));
	playlistNode.setAttribute("public", tracklist->isPublic() ? "true" : "false");
	playlistNode.setAttribute("created", "");
	{
		std::string userId {std::to_string(tracklist->getUser().id())};

		Wt::Auth::User authUser { db.getUserDatabase().findWithId(userId)};
		if (!authUser.isValid())
			throw Error {Error::CustomType::InternalError};

		playlistNode.setAttribute("owner", authUser.identity(Wt::Auth::Identity::LoginName).toUTF8());
	}

	return playlistNode;
}

Response
handleGetPlaylistRequest(RequestContext& context)
{
	// Mandatory params
	Id id {getMandatoryParameterAs<Id>(context.parameters, "id")};
	if (id.type != Id::Type::Playlist)
		throw Error {Error::CustomType::BadId};

	Wt::Dbo::Transaction transaction {context.db.getSession()};

	Database::User::pointer user {context.db.getUser(context.userName)};
	if (!user)
		throw Error {Error::Code::RequestedDataNotFound};

	Database::TrackList::pointer tracklist {Database::TrackList::getById(context.db.getSession(), id.value)};
	if (!tracklist)
		throw Error {Error::Code::RequestedDataNotFound};

	Response response {Response::createOkResponse()};
	Response::Node playlistNode {tracklistToResponseNode(tracklist, context.db)};

	auto entries {tracklist->getEntries()};
	for (const Database::TrackListEntry::pointer& entry : entries)
		playlistNode.addArrayChild("entry", trackToResponseNode(entry->getTrack()));

	response.addNode("playlist", playlistNode );

	return response;
}

Response
handleGetPlaylistsRequest(RequestContext& context)
{
	Wt::Dbo::Transaction transaction {context.db.getSession()};

	Database::User::pointer user {context.db.getUser(context.userName)};
	if (!user)
		throw Error {Error::Code::RequestedDataNotFound};

	Response response {Response::createOkResponse()};
	Response::Node& playlistsNode {response.createNode("playlists")};

	auto tracklists {Database::TrackList::getAll(context.db.getSession(), user, Database::TrackList::Type::Playlist)};
	for (const Database::TrackList::pointer& tracklist : tracklists)
		playlistsNode.addArrayChild("playlist", tracklistToResponseNode(tracklist, context.db));

	return response;
}

Response
handleGetSongsByGenreRequest(RequestContext& context)
{
	// Mandatory params
	std::string genre {getMandatoryParameterAs<std::string>(context.parameters, "genre")};

	// Optional params
	std::size_t size {getParameterAs<std::size_t>(context.parameters, "count").get_value_or(10)};
	size = std::min(size, std::size_t {500});

	std::size_t offset {getParameterAs<std::size_t>(context.parameters, "offset").get_value_or(0)};

	Wt::Dbo::Transaction transaction {context.db.getSession()};

	auto clusterType {Database::ClusterType::getByName(context.db.getSession(), CLUSTER_TYPE_GENRE)};
	if (!clusterType)
		throw Error {Error::Code::RequestedDataNotFound};

	auto cluster {clusterType->getCluster(genre)};
	if (!cluster)
		throw Error {Error::Code::RequestedDataNotFound};

	Response response {Response::createOkResponse()};
	Response::Node& songsByGenreNode {response.createNode("songsByGenre")};

	bool more;
	auto tracks {Database::Track::getByFilter(context.db.getSession(), {cluster.id()}, {}, offset, size, more)};
	for (const Database::Track::pointer& track : tracks)
		songsByGenreNode.addArrayChild("song", trackToResponseNode(track));

	return response;
}

Response
handleSearchRequestCommon(RequestContext& context, bool id3)
{
	// Mandatory params
	std::string query {getMandatoryParameterAs<std::string>(context.parameters, "query")};

	std::vector<std::string> keywords {splitString(query, " ")};

	// Optional params
	std::size_t artistCount {getParameterAs<std::size_t>(context.parameters, "artistCount").get_value_or(20)};
	std::size_t artistOffset {getParameterAs<std::size_t>(context.parameters, "artistOffset").get_value_or(0)};
	std::size_t albumCount {getParameterAs<std::size_t>(context.parameters, "albumCount").get_value_or(20)};
	std::size_t albumOffset {getParameterAs<std::size_t>(context.parameters, "albumOffset").get_value_or(0)};
	std::size_t songCount {getParameterAs<std::size_t>(context.parameters, "songCount").get_value_or(20)};
	std::size_t songOffset {getParameterAs<std::size_t>(context.parameters, "songOffset").get_value_or(0)};

	Wt::Dbo::Transaction transaction {context.db.getSession()};

	Response response {Response::createOkResponse()};
	Response::Node& searchResult2Node {response.createNode(id3 ? "searchResult3" : "searchResult2")};

	bool more;
	{
		auto artists {Database::Artist::getByFilter(context.db.getSession(), {}, keywords, artistOffset, artistCount, more)};
		for (const Database::Artist::pointer& artist : artists)
			searchResult2Node.addArrayChild("artist", artistToResponseNode(artist, id3));
	}

	{
		auto releases {Database::Release::getByFilter(context.db.getSession(), {}, keywords, albumOffset, albumCount, more)};
		for (const Database::Release::pointer& release : releases)
			searchResult2Node.addArrayChild("album", releaseToResponseNode(release, id3));
	}

	{
		auto tracks {Database::Track::getByFilter(context.db.getSession(), {}, keywords, songOffset, songCount, more)};
		for (const Database::Track::pointer& track : tracks)
			searchResult2Node.addArrayChild("song", trackToResponseNode(track));
	}

	return response;
}

Response
handleSearch2Request(RequestContext& context)
{
	return handleSearchRequestCommon(context, false /* no id3 */);
}

Response
handleSearch3Request(RequestContext& context)
{
	return handleSearchRequestCommon(context, true /* id3 */);
}

Response
handleUpdatePlaylistRequest(RequestContext& context)
{
	// Mandatory params
	Id id {getMandatoryParameterAs<Id>(context.parameters, "playlistId")};
	if (id.type != Id::Type::Playlist)
		throw Error {Error::CustomType::BadId};

	// Optional parameters
	auto name {getParameterAs<std::string>(context.parameters, "name")};
	auto isPublic {getParameterAs<bool>(context.parameters, "public")};

	std::vector<Id> trackIdsToAdd {getMultiParametersAs<Id>(context.parameters, "songIdToAdd")};
	if (!std::all_of(std::cbegin(trackIdsToAdd), std::cend(trackIdsToAdd), [](const Id& id) { return id.type == Id::Type::Track; }))
		throw Error {Error::CustomType::BadId};

	std::vector<std::size_t> trackPositionsToRemove {getMultiParametersAs<std::size_t>(context.parameters, "songIndexToRemove")};

	Wt::Dbo::Transaction transaction {context.db.getSession()};

	Database::User::pointer user {context.db.getUser(context.userName)};
	if (!user)
		throw Error {Error::Code::RequestedDataNotFound};

	Database::TrackList::pointer tracklist {Database::TrackList::getById(context.db.getSession(), id.value)};
	if (!tracklist
		|| tracklist->getUser() != user
		|| tracklist->getType() != Database::TrackList::Type::Playlist)
	{
		throw Error {Error::Code::RequestedDataNotFound};
	}

	if (name)
		tracklist.modify()->setName(*name);

	if (isPublic)
		tracklist.modify()->setIsPublic(*isPublic);

	{
		// Remove from end to make indexes stable
		std::sort(std::begin(trackPositionsToRemove), std::end(trackPositionsToRemove), std::greater<std::size_t>());

		for (std::size_t trackPositionToRemove : trackPositionsToRemove)
		{
			auto entry {tracklist->getEntry(trackPositionToRemove)};
			if (entry)
				entry.remove();
		}
	}

	// Add tracks
	for (const Id& trackIdToAdd : trackIdsToAdd)
	{
		Database::Track::pointer track {Database::Track::getById(context.db.getSession(), trackIdToAdd.value)};
		if (!track)
			continue;

		Database::TrackListEntry::create(context.db.getSession(), track, tracklist );
	}

	return Response::createOkResponse();
}

static
std::shared_ptr<Av::Transcoder>
createTranscoder(RequestContext& context)
{
	// Mandatory params
	Id id {getMandatoryParameterAs<Id>(context.parameters, "id")};

	// Optional params
	boost::optional<std::size_t> maxBitRate {getParameterAs<std::size_t>(context.parameters, "maxBitRate")};

	boost::filesystem::path trackPath;
	{
		Wt::Dbo::Transaction transaction {context.db.getSession()};

		Database::User::pointer user {context.db.getUser(context.userName)};
		if (!user)
			throw Error {Error::Code::RequestedDataNotFound};

		// "If set to zero, no limit is imposed"
		if (!maxBitRate || *maxBitRate == 0)
			maxBitRate = user->getAudioBitrate() / 1000;

		*maxBitRate = clamp(*maxBitRate, std::size_t {48}, user->getMaxAudioBitrate() / 1000);

		auto track {Database::Track::getById(context.db.getSession(), id.value)};
		if (!track)
			throw Error {Error::Code::RequestedDataNotFound};

		trackPath = track->getPath();
	}

	Av::TranscodeParameters parameters {};

	parameters.stripMetadata = false; // Since it can be cached and some players read the metadata from the downloaded file
	parameters.bitrate = *maxBitRate * 1000;
	parameters.encoding = Av::Encoding::MP3;

	return std::make_shared<Av::Transcoder>(trackPath, parameters);
}

void
handleStream(RequestContext& context, Wt::Http::ResponseContinuation* continuation, Wt::Http::Response& response)
{
	// TODO store only weak ptrs and use a ring container to store shared_ptr?
	std::shared_ptr<Av::Transcoder> transcoder;

	if (!continuation)
	{
		transcoder = createTranscoder(context);
		response.setMimeType(Av::encodingToMimetype(Av::Encoding::MP3));
		transcoder->start();
	}
	else
	{
		transcoder = Wt::cpp17::any_cast<std::shared_ptr<Av::Transcoder>>(continuation->data());
	}

	if (!transcoder)
		throw Error {Error::CustomType::InternalError};

	if (!transcoder->isComplete())
	{
		static constexpr std::size_t chunkSize {65536*4};

		std::vector<unsigned char> data;
		data.reserve(chunkSize);

		transcoder->process(data, chunkSize);

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
}

void
handleGetCoverArt(RequestContext& context, Wt::Http::ResponseContinuation* continuation, Wt::Http::Response& response)
{
	// Mandatory params
	Id id {getMandatoryParameterAs<Id>(context.parameters, "id")};

	std::size_t size {getParameterAs<std::size_t>(context.parameters, "size").get_value_or(256)};

	size = clamp(size, std::size_t {32}, std::size_t {1024});

	std::vector<uint8_t> cover;
	switch (id.type)
	{
		case Id::Type::Track:
			cover = getServices().coverArtGrabber->getFromTrack(context.db.getSession(), id.value, Image::Format::JPEG, size);
			break;
		case Id::Type::Release:
			cover = getServices().coverArtGrabber->getFromRelease(context.db.getSession(), id.value, Image::Format::JPEG, size);
			break;
		default:
			throw Error {Error::CustomType::BadId};
	}

	response.setMimeType( Image::format_to_mimeType(Image::Format::JPEG) );
	response.out().write(reinterpret_cast<const char *>(&cover[0]), cover.size());
}

} // namespace api::subsonic

