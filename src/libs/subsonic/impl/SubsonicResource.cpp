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
#include <ctime>
#include <iomanip>
#include <unordered_map>

#include <Wt/WLocalDateTime.h>

#include "lmscore/auth/IPasswordService.hpp"
#include "lmscore/auth/IEnvService.hpp"
#include "lmscore/cover/ICoverService.hpp"
#include "database/Artist.hpp"
#include "database/Cluster.hpp"
#include "database/Db.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/TrackBookmark.hpp"
#include "database/TrackList.hpp"
#include "database/User.hpp"
#include "recommendation/IEngine.hpp"
#include "scrobbling/IScrobbling.hpp"
#include "utils/IConfig.hpp"
#include "utils/Logger.hpp"
#include "utils/Random.hpp"
#include "utils/Service.hpp"
#include "utils/String.hpp"
#include "utils/Utils.hpp"
#include "ParameterParsing.hpp"
#include "ProtocolVersion.hpp"
#include "RequestContext.hpp"
#include "Scan.hpp"
#include "Stream.hpp"
#include "SubsonicId.hpp"
#include "SubsonicResponse.hpp"

using namespace Database;

static const std::string	genreClusterName {"GENRE"};
static const std::string	reportedStarredDate {"2000-01-01T00:00:00"};
static const std::string	reportedDummyDate {"2000-01-01T00:00:00"};
static const unsigned long long	reportedDummyDateULong {946684800000ULL}; // 2000-01-01T00:00:00 UTC




namespace API::Subsonic
{

std::unique_ptr<Wt::WResource>
createSubsonicResource(Database::Db& db)
{
	return std::make_unique<SubsonicResource>(db);
}

static
void
checkSetPasswordImplemented()
{
	Auth::IPasswordService* passwordService {Service<Auth::IPasswordService>::get()};
	if (!passwordService || !passwordService->canSetPasswords())
		throw NotImplementedGenericError {};
}

static
std::string
makeNameFilesystemCompatible(const std::string& name)
{
	return StringUtils::replaceInString(name, "/", "_");
}

static
std::string
decodePasswordIfNeeded(const std::string& password)
{
	if (password.find("enc:") == 0)
	{
		auto decodedPassword {StringUtils::stringFromHex(password.substr(4))};
		if (!decodedPassword)
			return password; // fallback on plain password

		return *decodedPassword;
	}

	return password;
}

static
std::unordered_map<std::string, ProtocolVersion>
readConfigProtocolVersions()
{
	std::unordered_map<std::string, ProtocolVersion> res;

	Service<IConfig>::get()->visitStrings("api-subsonic-report-old-server-protocol",
			[&](std::string_view client)
			{
				res.emplace(std::string {client}, ProtocolVersion {1, 12, 0});
			}, {"DSub"});

	return res;
}

SubsonicResource::SubsonicResource(Db& db)
: _serverProtocolVersionsByClient {readConfigProtocolVersions()}
, _db {db}
{
}

static
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

static
void
checkUserIsMySelfOrAdmin(RequestContext& context, const std::string& username)
{
	User::pointer currentUser {User::getById(context.dbSession, context.userId)};
	if (!currentUser)
		throw RequestedDataNotFoundError {};

	if (currentUser->getLoginName() != username	&& !currentUser->isAdmin())
		throw UserNotAuthorizedError {};
}

static
void
checkUserTypeIsAllowed(RequestContext& context, EnumSet<Database::UserType> allowedUserTypes)
{
	auto transaction {context.dbSession.createSharedTransaction()};

	User::pointer currentUser {User::getById(context.dbSession, context.userId)};
	if (!currentUser)
		throw RequestedDataNotFoundError {};

	if (!allowedUserTypes.contains(currentUser->getType()))
		throw UserNotAuthorizedError {};
}

static
std::string
getArtistNames(const std::vector<Artist::pointer>& artists)
{
	if (artists.size() == 1)
		return artists.front()->getName();

	std::vector<std::string> names;
	names.resize(artists.size());

	std::transform(std::cbegin(artists), std::cend(artists), std::begin(names),
			[](const Artist::pointer& artist)
			{
				return artist->getName();
			});

	return StringUtils::joinStrings(names, ", ");
}

static
std::string
getTrackPath(const Track::pointer& track)
{
	std::string path;

	// The track path has to be relative from the root

	const auto release {track->getRelease()};
	if (release)
	{
		auto artists {release->getReleaseArtists()};
		if (artists.empty())
			artists = release->getArtists();

		if (artists.size() > 1)
			path = "Various Artists/";
		else if (artists.size() == 1)
			path = makeNameFilesystemCompatible(artists.front()->getName()) + "/";

		path += makeNameFilesystemCompatible(track->getRelease()->getName()) + "/";
	}

	if (track->getDiscNumber())
		path += std::to_string(*track->getDiscNumber()) + "-";
	if (track->getTrackNumber())
		path += std::to_string(*track->getTrackNumber()) + "-";

	path += makeNameFilesystemCompatible(track->getName());

	if (track->getPath().has_extension())
		path += track->getPath().extension();

	return path;
}

static
std::string_view
formatToSuffix(AudioFormat format)
{
	switch (format)
	{
		case AudioFormat::MP3:			return "mp3";
		case AudioFormat::OGG_OPUS:		return "opus";
		case AudioFormat::MATROSKA_OPUS:	return "mka";
		case AudioFormat::OGG_VORBIS:		return "ogg";
		case AudioFormat::WEBM_VORBIS:		return "webm";
	}

	return "";
}

static
std::string
dateTimeToCreatedString(const Wt::WDateTime& dateTime)
{
		const std::time_t t {dateTime.toTime_t()};
		std::tm gmTime;
		std::ostringstream oss;
		oss << std::put_time(::gmtime_r(&t, &gmTime), "%FT%T");
		return oss.str();
}

static
Response::Node
trackToResponseNode(const Track::pointer& track, Session& dbSession, const User::pointer& user)
{
	Response::Node trackResponse;

	trackResponse.setAttribute("id", idToString(track->getId()));
	trackResponse.setAttribute("isDir", false);
	trackResponse.setAttribute("title", track->getName());
	if (track->getTrackNumber())
		trackResponse.setAttribute("track", *track->getTrackNumber());
	if (track->getDiscNumber())
		trackResponse.setAttribute("discNumber", *track->getDiscNumber());
	if (track->getYear())
		trackResponse.setAttribute("year", *track->getYear());

	trackResponse.setAttribute("path", getTrackPath(track));
	{
		std::error_code ec;
		const auto fileSize {std::filesystem::file_size(track->getPath(), ec)};
		if (!ec)
			trackResponse.setAttribute("size", fileSize);
	}

	if (track->getPath().has_extension())
	{
		auto extension {track->getPath().extension()};
		trackResponse.setAttribute("suffix", extension.string().substr(1));
	}

	if (user->getSubsonicTranscodeEnable())
		trackResponse.setAttribute("transcodedSuffix", formatToSuffix(user->getSubsonicTranscodeFormat()));

	trackResponse.setAttribute("coverArt", idToString(track->getId()));

	const std::vector<Artist::pointer>& artists {track->getArtists({TrackArtistLinkType::Artist})};
	if (!artists.empty())
	{
		trackResponse.setAttribute("artist", getArtistNames(artists));

		if (artists.size() == 1)
			trackResponse.setAttribute("artistId", idToString(artists.front()->getId()));
	}

	if (track->getRelease())
	{
		trackResponse.setAttribute("album", track->getRelease()->getName());
		trackResponse.setAttribute("albumId", idToString(track->getRelease()->getId()));
		trackResponse.setAttribute("parent", idToString(track->getRelease()->getId()));
	}

	trackResponse.setAttribute("duration", std::chrono::duration_cast<std::chrono::seconds>(track->getDuration()).count());
	trackResponse.setAttribute("type", "music");
	trackResponse.setAttribute("created", dateTimeToCreatedString(track->getLastWritten()));

	if (user->hasStarredTrack(track))
		trackResponse.setAttribute("starred", reportedStarredDate);

	// Report the first GENRE for this track
	ClusterType::pointer clusterType {ClusterType::getByName(dbSession, genreClusterName)};
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
trackBookmarkToResponseNode(const TrackBookmark::pointer& trackBookmark)
{
	Response::Node trackBookmarkNode;

	trackBookmarkNode.setAttribute("position", trackBookmark->getOffset().count());
	if (!trackBookmark->getComment().empty())
		trackBookmarkNode.setAttribute("comment", trackBookmark->getComment());
	trackBookmarkNode.setAttribute("created", reportedDummyDate);
	trackBookmarkNode.setAttribute("changed", reportedDummyDate);
	trackBookmarkNode.setAttribute("username", trackBookmark->getUser()->getLoginName());

	return trackBookmarkNode;
}

static
Response::Node
releaseToResponseNode(const Release::pointer& release, Session& dbSession, const User::pointer& user, bool id3)
{
	Response::Node albumNode;

	if (id3)
	{
		albumNode.setAttribute("name", release->getName());
		albumNode.setAttribute("songCount", release->getTracksCount());
		albumNode.setAttribute("duration", std::chrono::duration_cast<std::chrono::seconds>(release->getDuration()).count());
	}
	else
	{
		albumNode.setAttribute("title", release->getName());
		albumNode.setAttribute("isDir", true);
	}

	albumNode.setAttribute("created", dateTimeToCreatedString(release->getLastWritten()));
	albumNode.setAttribute("id", idToString(release->getId()));
	albumNode.setAttribute("coverArt", idToString(release->getId()));
	auto releaseYear {release->getReleaseYear()};
	if (releaseYear)
		albumNode.setAttribute("year", *releaseYear);

	auto artists {release->getReleaseArtists()};
	if (artists.empty())
		artists = release->getArtists();

	if (artists.empty() && !id3)
	{
		albumNode.setAttribute("parent", idToString(RootId {}));
	}
	else if (!artists.empty())
	{
		albumNode.setAttribute("artist", getArtistNames(artists));

		if (artists.size() == 1)
		{
			albumNode.setAttribute(id3 ? "artistId" : "parent", idToString(artists.front()->getId()));
		}
		else
		{
			if (!id3)
				albumNode.setAttribute("parent", idToString(RootId {}));
		}
	}

	if (id3)
	{
		// Report the first GENRE for this track
		ClusterType::pointer clusterType {ClusterType::getByName(dbSession, genreClusterName)};
		if (clusterType)
		{
			auto clusters {release->getClusterGroups({clusterType}, 1)};
			if (!clusters.empty() && !clusters.front().empty())
				albumNode.setAttribute("genre", clusters.front().front()->getName());
		}
	}

	if (user->hasStarredRelease(release))
		albumNode.setAttribute("starred", reportedStarredDate);

	return albumNode;
}

static
Response::Node
artistToResponseNode(const User::pointer& user, const Artist::pointer& artist, bool id3)
{
	Response::Node artistNode;

	artistNode.setAttribute("id", idToString(artist->getId()));
	artistNode.setAttribute("name", artist->getName());

	if (id3)
		artistNode.setAttribute("albumCount", artist->getReleaseCount());

	if (user->hasStarredArtist(artist))
		artistNode.setAttribute("starred", reportedStarredDate);

	return artistNode;
}

static
Response::Node
clusterToResponseNode(const Cluster::pointer& cluster)
{
	Response::Node clusterNode;

	clusterNode.setValue(cluster->getName());
	clusterNode.setAttribute("songCount", cluster->getTracksCount());
	clusterNode.setAttribute("albumCount", cluster->getReleasesCount());

	return clusterNode;
}

static
Response::Node
userToResponseNode(const User::pointer& user)
{
	Response::Node userNode;

	userNode.setAttribute("username", user->getLoginName());
	userNode.setAttribute("scrobblingEnabled", true);
	userNode.setAttribute("adminRole", user->isAdmin());
	userNode.setAttribute("settingsRole", true);
	userNode.setAttribute("downloadRole", true);
	userNode.setAttribute("uploadRole", false);
	userNode.setAttribute("playlistRole", true);
	userNode.setAttribute("coverArtRole", false);
	userNode.setAttribute("commentRole", false);
	userNode.setAttribute("podcastRole", false);
	userNode.setAttribute("streamRole", true);
	userNode.setAttribute("jukeboxRole", false);
	userNode.setAttribute("shareRole", false);

	Response::Node folder;
	folder.setValue("0");
	userNode.addArrayChild("folder", std::move(folder));

	return userNode;
}

static
Response
handlePingRequest(RequestContext& context)
{
	return Response::createOkResponse(context.serverProtocolVersion);
}

static
Response
handleChangePassword(RequestContext& context)
{
	std::string username {getMandatoryParameterAs<std::string>(context.parameters, "username")};
	std::string password {decodePasswordIfNeeded(getMandatoryParameterAs<std::string>(context.parameters, "password"))};

	try
	{
		Database::UserId userId;
		{
			auto transaction {context.dbSession.createSharedTransaction()};

			checkUserIsMySelfOrAdmin(context, username);

			User::pointer user {User::getByLoginName(context.dbSession, username)};
			if (!user)
				throw UserNotAuthorizedError {};

			userId = user->getId();
		}

		Service<Auth::IPasswordService>::get()->setPassword(userId, password);
	}
	catch (const Auth::PasswordMustMatchLoginNameException&)
	{
		throw PasswordMustMatchLoginNameGenericError {};
	}
	catch (const Auth::PasswordTooWeakException&)
	{
		throw PasswordTooWeakGenericError {};
	}
	catch (const Auth::Exception& authException)
	{
		throw UserNotAuthorizedError {};
	}

	return Response::createOkResponse(context.serverProtocolVersion);
}

static
Response
handleCreatePlaylistRequest(RequestContext& context)
{
	// Optional params
	const auto id {getParameterAs<TrackListId>(context.parameters, "playlistId")};
	auto name {getParameterAs<std::string>(context.parameters, "name")};

	std::vector<TrackId> trackIds {getMultiParametersAs<TrackId>(context.parameters, "songId")};

	if (!name && !id)
		throw RequiredParameterMissingError {"name or id"};

	auto transaction {context.dbSession.createUniqueTransaction()};

	User::pointer user {User::getById(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	TrackList::pointer tracklist;
	if (id)
	{
		tracklist = TrackList::getById(context.dbSession, *id);
		if (!tracklist
			|| tracklist->getUser() != user
			|| tracklist->getType() != TrackList::Type::Playlist)
		{
			throw RequestedDataNotFoundError {};
		}

		if (name)
			tracklist.modify()->setName(*name);
	}
	else
	{
		tracklist = TrackList::create(context.dbSession, *name, TrackList::Type::Playlist, false, user);
	}

	for (const TrackId trackId : trackIds)
	{
		Track::pointer track {Track::getById(context.dbSession, trackId)};
		if (!track)
			continue;

		TrackListEntry::create(context.dbSession, track, tracklist );
	}

	return Response::createOkResponse(context.serverProtocolVersion);
}

static
Response
handleCreateUserRequest(RequestContext& context)
{
	std::string username {getMandatoryParameterAs<std::string>(context.parameters, "username")};
	std::string password {decodePasswordIfNeeded(getMandatoryParameterAs<std::string>(context.parameters, "password"))};
	// Just ignore all the other fields as we don't handle them

	Database::UserId userId;
	{
		auto transaction {context.dbSession.createUniqueTransaction()};

		User::pointer user {User::getByLoginName(context.dbSession, username)};
		if (user)
			throw UserAlreadyExistsGenericError {};

		user = User::create(context.dbSession, username);
		userId = user->getId();
	}

	auto removeCreatedUser {[&]()
	{
		auto transaction {context.dbSession.createUniqueTransaction()};
		User::pointer user {User::getById(context.dbSession, userId)};
		if (user)
			user.remove();
	}};

	try
	{
		Service<Auth::IPasswordService>::get()->setPassword(userId, password);
	}
	catch (const Auth::PasswordMustMatchLoginNameException&)
	{
		removeCreatedUser();
		throw PasswordMustMatchLoginNameGenericError {};
	}
	catch (const Auth::PasswordTooWeakException&)
	{
		removeCreatedUser();
		throw PasswordTooWeakGenericError {};
	}
	catch (const Auth::Exception& exception)
	{
		removeCreatedUser();
		throw UserNotAuthorizedError {};
	}

	return Response::createOkResponse(context.serverProtocolVersion);
}

static
Response
handleDeletePlaylistRequest(RequestContext& context)
{
	TrackListId id {getMandatoryParameterAs<TrackListId>(context.parameters, "id")};

	auto transaction {context.dbSession.createUniqueTransaction()};

	User::pointer user {User::getById(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	TrackList::pointer tracklist {TrackList::getById(context.dbSession, id)};
	if (!tracklist
		|| tracklist->getUser() != user
		|| tracklist->getType() != TrackList::Type::Playlist)
	{
		throw RequestedDataNotFoundError {};
	}

	tracklist.remove();

	return Response::createOkResponse(context.serverProtocolVersion);
}

static
Response
handleDeleteUserRequest(RequestContext& context)
{
	std::string username {getMandatoryParameterAs<std::string>(context.parameters, "username")};

	auto transaction {context.dbSession.createUniqueTransaction()};

	User::pointer user {User::getByLoginName(context.dbSession, username)};
	if (!user)
		throw RequestedDataNotFoundError {};

	// cannot delete ourself
	if (user->getId() == context.userId)
		throw UserNotAuthorizedError {};

	user.remove();

	return Response::createOkResponse(context.serverProtocolVersion);
}

static
Response
handleGetLicenseRequest(RequestContext& context)
{
	Response response {Response::createOkResponse(context.serverProtocolVersion)};

	Response::Node& licenseNode {response.createNode("license")};
	licenseNode.setAttribute("licenseExpires", "2025-09-03T14:46:43");
	licenseNode.setAttribute("email", "foo@bar.com");
	licenseNode.setAttribute("valid", true);

	return response;
}

static
Response
handleGetRandomSongsRequest(RequestContext& context)
{
	// Optional params
	std::size_t size {getParameterAs<std::size_t>(context.parameters, "size").value_or(50)};
	size = std::min(size, std::size_t {500});

	auto transaction {context.dbSession.createSharedTransaction()};

	User::pointer user {User::getById(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	auto tracks {Track::getAllRandom(context.dbSession, {}, size)};

	Response response {Response::createOkResponse(context.serverProtocolVersion)};

	Response::Node& randomSongsNode {response.createNode("randomSongs")};
	for (const Track::pointer& track : tracks)
		randomSongsNode.addArrayChild("song", trackToResponseNode(track, context.dbSession, user));

	return response;
}

static
Response
handleGetAlbumListRequestCommon(const RequestContext& context, bool id3)
{
	// Mandatory params
	const std::string type {getMandatoryParameterAs<std::string>(context.parameters, "type")};

	// Optional params
	const std::size_t size {getParameterAs<std::size_t>(context.parameters, "size").value_or(10)};
	const std::size_t offset {getParameterAs<std::size_t>(context.parameters, "offset").value_or(0)};

	const Range range {offset, size};

	std::vector<Release::pointer> releases;
	Scrobbling::IScrobbling& scrobbling {*Service<Scrobbling::IScrobbling>::get()};

	auto transaction {context.dbSession.createSharedTransaction()};

	User::pointer user {User::getById(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	if (type == "alphabeticalByName")
	{
		releases = Release::getAll(context.dbSession, range);
	}
	else if (type == "alphabeticalByArtist")
	{
		releases = Release::getAllOrderedByArtist(context.dbSession, offset, size);
	}
	else if (type == "byGenre")
	{
		// Mandatory param
		std::string genre {getMandatoryParameterAs<std::string>(context.parameters, "genre")};

		ClusterType::pointer clusterType {ClusterType::getByName(context.dbSession, genreClusterName)};
		if (clusterType)
		{
			Cluster::pointer cluster {clusterType->getCluster(genre)};
			if (cluster)
			{
				bool more;
				releases = Release::getByFilter(context.dbSession, {cluster->getId()}, {}, range, more);
			}
		}
	}
	else if (type == "byYear")
	{
		int fromYear {getMandatoryParameterAs<int>(context.parameters, "fromYear")};
		int toYear {getMandatoryParameterAs<int>(context.parameters, "toYear")};

		releases = Release::getByYear(context.dbSession, fromYear, toYear, range);
	}
	else if (type == "frequent")
	{
		bool moreResults {};
		for (ReleaseId releaseId : scrobbling.getTopReleases(context.userId, {}, range, moreResults))
		{
			if (Release::pointer release {Release::getById(context.dbSession, releaseId)})
				releases.push_back(release );
		}
	}
	else if (type == "newest")
	{
		bool moreResults {};
		releases = Release::getLastWritten(context.dbSession, std::nullopt, {}, range, moreResults);
	}
	else if (type == "random")
	{
		// Random results are paginated, but there is no acceptable way to handle the pagination params without repeating some albums
		releases = Release::getAllRandom(context.dbSession, {}, size);
	}
	else if (type == "recent")
	{
		bool moreResults {};
		for (ReleaseId releaseId : scrobbling.getRecentReleases(context.userId, {}, range, moreResults))
		{
			if (Release::pointer release {Release::getById(context.dbSession, releaseId)})
				releases.push_back(release );
		}
	}
	else if (type == "starred")
	{
		bool moreResults {};
		releases = Release::getStarred(context.dbSession, user, {}, range, moreResults);
	}
	else
		throw NotImplementedGenericError {};

	Response response {Response::createOkResponse(context.serverProtocolVersion)};
	Response::Node& albumListNode {response.createNode(id3 ? "albumList2" : "albumList")};

	for (const Release::pointer& release : releases)
		albumListNode.addArrayChild("album", releaseToResponseNode(release, context.dbSession, user, id3));

	return response;
}

static
Response
handleGetAlbumListRequest(RequestContext& context)
{
	return handleGetAlbumListRequestCommon(context, false /* no id3 */);
}

static
Response
handleGetAlbumList2Request(RequestContext& context)
{
	return handleGetAlbumListRequestCommon(context, true /* id3 */);
}

static
Response
handleGetAlbumRequest(RequestContext& context)
{
	// Mandatory params
	ReleaseId id {getMandatoryParameterAs<ReleaseId>(context.parameters, "id")};

	auto transaction {context.dbSession.createSharedTransaction()};

	Release::pointer release {Release::getById(context.dbSession, id)};
	if (!release)
		throw RequestedDataNotFoundError {};

	User::pointer user {User::getById(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	Response response {Response::createOkResponse(context.serverProtocolVersion)};
	Response::Node releaseNode {releaseToResponseNode(release, context.dbSession, user, true /* id3 */)};

	auto tracks {release->getTracks()};
	for (const Track::pointer& track : tracks)
		releaseNode.addArrayChild("song", trackToResponseNode(track, context.dbSession, user));

	response.addNode("album", std::move(releaseNode));

	return response;
}

static
Response
handleGetArtistRequest(RequestContext& context)
{
	// Mandatory params
	ArtistId id {getMandatoryParameterAs<ArtistId>(context.parameters, "id")};

	auto transaction {context.dbSession.createSharedTransaction()};

	Artist::pointer artist {Artist::getById(context.dbSession, id)};
	if (!artist)
		throw RequestedDataNotFoundError {};

	User::pointer user {User::getById(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	Response response {Response::createOkResponse(context.serverProtocolVersion)};
	Response::Node artistNode {artistToResponseNode(user, artist, true /* id3 */)};

	auto releases {artist->getReleases()};
	for (const Release::pointer& release : releases)
		artistNode.addArrayChild("album", releaseToResponseNode(release, context.dbSession, user, true /* id3 */));

	response.addNode("artist", std::move(artistNode));

	return response;
}

static
Response
handleGetArtistInfoRequestCommon(RequestContext& context, bool id3)
{
	// Mandatory params
	ArtistId id {getMandatoryParameterAs<ArtistId>(context.parameters, "id")};

	// Optional params
	std::size_t count {getParameterAs<std::size_t>(context.parameters, "count").value_or(20)};

	Response response {Response::createOkResponse(context.serverProtocolVersion)};
	Response::Node& artistInfoNode {response.createNode(id3 ? "artistInfo2" : "artistInfo")};

	{
		auto transaction {context.dbSession.createSharedTransaction()};

		Artist::pointer artist {Artist::getById(context.dbSession, id)};
		if (!artist)
			throw RequestedDataNotFoundError {};

		std::optional<UUID> artistMBID {artist->getMBID()};
		if (artistMBID)
			artistInfoNode.createChild("musicBrainzId").setValue(artistMBID->getAsString());
	}

	auto similarArtistsId {Service<Recommendation::IEngine>::get()->getSimilarArtists(id, {TrackArtistLinkType::Artist, TrackArtistLinkType::ReleaseArtist}, count)};

	{
		auto transaction {context.dbSession.createSharedTransaction()};

		User::pointer user {User::getById(context.dbSession, context.userId)};
		if (!user)
			throw UserNotAuthorizedError {};

		for ( const ArtistId similarArtistId : similarArtistsId )
		{
			Artist::pointer similarArtist {Artist::getById(context.dbSession, similarArtistId)};
			if (similarArtist)
				artistInfoNode.addArrayChild("similarArtist", artistToResponseNode(user, similarArtist, id3));
		}
	}

	return response;
}

static
Response
handleGetArtistInfoRequest(RequestContext& context)
{
	return handleGetArtistInfoRequestCommon(context, false /* no id3 */);
}

static
Response
handleGetArtistInfo2Request(RequestContext& context)
{
	return handleGetArtistInfoRequestCommon(context, true /* id3 */);
}

static
Response
handleGetArtistsRequest(RequestContext& context)
{
	Response response {Response::createOkResponse(context.serverProtocolVersion)};

	Response::Node& artistsNode {response.createNode("artists")};
	artistsNode.setAttribute("ignoredArticles", "");
	artistsNode.setAttribute("lastModified", reportedDummyDateULong);

	Response::Node& indexNode {artistsNode.createArrayChild("index")};
	indexNode.setAttribute("name", "?");

	auto transaction {context.dbSession.createSharedTransaction()};

	User::pointer user {User::getById(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	std::optional<TrackArtistLinkType> linkType;
	switch (user->getSubsonicArtistListMode())
	{
		case User::SubsonicArtistListMode::AllArtists:
			break;
		case User::SubsonicArtistListMode::ReleaseArtists:
			linkType = TrackArtistLinkType::ReleaseArtist;
			break;
		case User::SubsonicArtistListMode::TrackArtists:
			linkType = TrackArtistLinkType::Artist;
			break;
	}

	bool more {};
	const std::vector<Artist::pointer> artists {Artist::getByFilter(context.dbSession,
			{},
			{},
			linkType,
			Artist::SortMethod::BySortName,
			std::nullopt, more)};
	for (const Artist::pointer& artist : artists)
		indexNode.addArrayChild("artist", artistToResponseNode(user, artist, true /* id3 */));

	return response;
}

static
Response
handleGetMusicDirectoryRequest(RequestContext& context)
{
	// Mandatory params
	const auto artistId {getParameterAs<ArtistId>(context.parameters, "id")};
	const auto releaseId {getParameterAs<ReleaseId>(context.parameters, "id")};
	const auto trackId {getParameterAs<TrackId>(context.parameters, "id")};
	const auto root {getParameterAs<RootId>(context.parameters, "id")};

	if (!root && !artistId && !releaseId && !trackId)
		throw BadParameterGenericError {"id"};

	Response response {Response::createOkResponse(context.serverProtocolVersion)};
	Response::Node& directoryNode {response.createNode("directory")};

	auto transaction {context.dbSession.createSharedTransaction()};

	User::pointer user {User::getById(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	if (root)
	{
		directoryNode.setAttribute("id", idToString(RootId {}));
		directoryNode.setAttribute("name", "Music");

		bool moreResults{};
		auto artists {Artist::getAll(context.dbSession, Artist::SortMethod::BySortName, std::nullopt, moreResults)};
		for (const Artist::pointer& artist : artists)
			directoryNode.addArrayChild("child", artistToResponseNode(user, artist, false /* no id3 */));
	}
	else if (artistId)
	{
		directoryNode.setAttribute("id", idToString(*artistId));

		auto artist {Artist::getById(context.dbSession, *artistId)};
		if (!artist)
			throw RequestedDataNotFoundError {};

		directoryNode.setAttribute("name", makeNameFilesystemCompatible(artist->getName()));

		auto releases {artist->getReleases()};
		for (const Release::pointer& release : releases)
			directoryNode.addArrayChild("child", releaseToResponseNode(release, context.dbSession, user, false /* no id3 */));
	}
	else if (releaseId)
	{
		directoryNode.setAttribute("id", idToString(*releaseId));

		auto release {Release::getById(context.dbSession, *releaseId)};
		if (!release)
				throw RequestedDataNotFoundError {};

		directoryNode.setAttribute("name", makeNameFilesystemCompatible(release->getName()));

		auto tracks {release->getTracks()};
		for (const Track::pointer& track : tracks)
			directoryNode.addArrayChild("child", trackToResponseNode(track, context.dbSession, user));
	}
	else
		throw BadParameterGenericError {"id"};

	return response;
}

static
Response
handleGetMusicFoldersRequest(RequestContext& context)
{
	Response response {Response::createOkResponse(context.serverProtocolVersion)};
	Response::Node& musicFoldersNode {response.createNode("musicFolders")};

	Response::Node& musicFolderNode {musicFoldersNode.createArrayChild("musicFolder")};
	musicFolderNode.setAttribute("id", "0");
	musicFolderNode.setAttribute("name", "Music");

	return response;
}

static
Response
handleGetGenresRequest(RequestContext& context)
{
	Response response {Response::createOkResponse(context.serverProtocolVersion)};

	Response::Node& genresNode {response.createNode("genres")};

	auto transaction {context.dbSession.createSharedTransaction()};

	const ClusterType::pointer clusterType {ClusterType::getByName(context.dbSession, genreClusterName)};
	if (clusterType)
	{
		const auto clusters {clusterType->getClusters()};

		for (const Cluster::pointer& cluster : clusters)
			genresNode.addArrayChild("genre", clusterToResponseNode(cluster));
	}

	return response;
}

static
Response
handleGetIndexesRequest(RequestContext& context)
{
	Response response {Response::createOkResponse(context.serverProtocolVersion)};

	Response::Node& artistsNode {response.createNode("indexes")};
	artistsNode.setAttribute("ignoredArticles", "");
	artistsNode.setAttribute("lastModified", reportedDummyDateULong);

	Response::Node& indexNode {artistsNode.createArrayChild("index")};
	indexNode.setAttribute("name", "?");

	auto transaction {context.dbSession.createSharedTransaction()};

	User::pointer user {User::getById(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	std::optional<TrackArtistLinkType> linkType;
	switch (user->getSubsonicArtistListMode())
	{
		case User::SubsonicArtistListMode::AllArtists:
			break;
		case User::SubsonicArtistListMode::ReleaseArtists:
			linkType = TrackArtistLinkType::ReleaseArtist;
			break;
		case User::SubsonicArtistListMode::TrackArtists:
			linkType = TrackArtistLinkType::Artist;
			break;
	}

	bool more {};
	const std::vector<Artist::pointer> artists {Artist::getByFilter(context.dbSession,
			{},
			{},
			linkType,
			Artist::SortMethod::BySortName,
			std::nullopt, more)};
	for (const Artist::pointer& artist : artists)
		indexNode.addArrayChild("artist", artistToResponseNode(user, artist, false /* no id3 */));

	return response;
}

static
Response
handleGetSimilarSongsRequestCommon(RequestContext& context, bool id3)
{
	// Mandatory params
	const ArtistId artistId {getMandatoryParameterAs<ArtistId>(context.parameters, "id")};

	// Optional params
	std::size_t count {getParameterAs<std::size_t>(context.parameters, "count").value_or(50)};

	const auto similarArtistIds {Service<Recommendation::IEngine>::get()->getSimilarArtists(artistId, {TrackArtistLinkType::Artist, TrackArtistLinkType::ReleaseArtist}, 5)};

	auto transaction {context.dbSession.createSharedTransaction()};

	const Artist::pointer artist {Artist::getById(context.dbSession, artistId)};
	if (!artist)
		throw RequestedDataNotFoundError {};

	const User::pointer user {User::getById(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	// "Returns a random collection of songs from the given artist and similar artists"
	auto tracks {artist->getRandomTracks(count / 2)};
	for (const ArtistId similarArtistId : similarArtistIds)
	{
		const Artist::pointer similarArtist {Artist::getById(context.dbSession, similarArtistId)};
		if (!similarArtist)
			continue;

		auto similarArtistTracks {similarArtist->getRandomTracks((count / 2) / 5)};

		tracks.insert(std::end(tracks),
				std::make_move_iterator(std::begin(similarArtistTracks)),
				std::make_move_iterator(std::end(similarArtistTracks)));
	}

	Random::shuffleContainer(tracks);

	Response response {Response::createOkResponse(context.serverProtocolVersion)};
	Response::Node& similarSongsNode {response.createNode(id3 ? "similarSongs2" : "similarSongs")};
	for (const Track::pointer& track : tracks)
		similarSongsNode.addArrayChild("song", trackToResponseNode(track, context.dbSession, user));

	return response;
}

static
Response
handleGetSimilarSongsRequest(RequestContext& context)
{
	return handleGetSimilarSongsRequestCommon(context, false /* no id3 */);
}

static
Response
handleGetSimilarSongs2Request(RequestContext& context)
{
	return handleGetSimilarSongsRequestCommon(context, true /* id3 */);
}

static
Response
handleGetStarredRequestCommon(RequestContext& context, bool id3)
{
	auto transaction {context.dbSession.createSharedTransaction()};

	User::pointer user {User::getById(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	Response response {Response::createOkResponse(context.serverProtocolVersion)};
	Response::Node& starredNode {response.createNode(id3 ? "starred2" : "starred")};

	{
		bool moreResults {};
		const auto artists {Artist::getStarred(context.dbSession, user, {}, std::nullopt, Artist::SortMethod::BySortName, std::nullopt, moreResults)};
		for (const Artist::pointer& artist : artists)
			starredNode.addArrayChild("artist", artistToResponseNode(user, artist, id3));
	}

	{
		bool moreResults {};
		const auto releases {Release::getStarred(context.dbSession, user, {}, std::nullopt, moreResults)};
		for (const Release::pointer& release : releases)
			starredNode.addArrayChild("album", releaseToResponseNode(release, context.dbSession, user, id3));
	}

	{
		bool moreResults {};
		const auto tracks {Track::getStarred(context.dbSession, user, {}, std::nullopt, moreResults)};
		for (const Track::pointer& track : tracks)
			starredNode.addArrayChild("song", trackToResponseNode(track, context.dbSession, user));
	}

	return response;

}

static
Response
handleGetStarredRequest(RequestContext& context)
{
	return handleGetStarredRequestCommon(context, false /* no id3 */);
}

static
Response
handleGetStarred2Request(RequestContext& context)
{
	return handleGetStarredRequestCommon(context, true /* id3 */);
}

static
Response::Node
tracklistToResponseNode(const TrackList::pointer& tracklist, Session&)
{
	Response::Node playlistNode;

	playlistNode.setAttribute("id", idToString(tracklist->getId()));
	playlistNode.setAttribute("name", tracklist->getName());
	playlistNode.setAttribute("songCount", tracklist->getCount());
	playlistNode.setAttribute("duration", std::chrono::duration_cast<std::chrono::seconds>(tracklist->getDuration()).count());
	playlistNode.setAttribute("public", tracklist->isPublic());
	playlistNode.setAttribute("created", reportedDummyDate);
	playlistNode.setAttribute("owner", tracklist->getUser()->getLoginName());

	return playlistNode;
}

static
Response
handleGetPlaylistRequest(RequestContext& context)
{
	// Mandatory params
	TrackListId trackListId {getMandatoryParameterAs<TrackListId>(context.parameters, "id")};

	auto transaction {context.dbSession.createSharedTransaction()};

	User::pointer user {User::getById(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	TrackList::pointer tracklist {TrackList::getById(context.dbSession, trackListId)};
	if (!tracklist)
		throw RequestedDataNotFoundError {};

	Response response {Response::createOkResponse(context.serverProtocolVersion)};
	Response::Node playlistNode {tracklistToResponseNode(tracklist, context.dbSession)};

	auto entries {tracklist->getEntries()};
	for (const TrackListEntry::pointer& entry : entries)
		playlistNode.addArrayChild("entry", trackToResponseNode(entry->getTrack(), context.dbSession, user));

	response.addNode("playlist", playlistNode );

	return response;
}

static
Response
handleGetPlaylistsRequest(RequestContext& context)
{
	auto transaction {context.dbSession.createSharedTransaction()};

	User::pointer user {User::getById(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	Response response {Response::createOkResponse(context.serverProtocolVersion)};
	Response::Node& playlistsNode {response.createNode("playlists")};

	auto tracklists {TrackList::getAll(context.dbSession, user, TrackList::Type::Playlist)};
	for (const TrackList::pointer& tracklist : tracklists)
		playlistsNode.addArrayChild("playlist", tracklistToResponseNode(tracklist, context.dbSession));

	return response;
}

static
Response
handleGetSongsByGenreRequest(RequestContext& context)
{
	// Mandatory params
	std::string genre {getMandatoryParameterAs<std::string>(context.parameters, "genre")};

	// Optional params
	std::size_t size {getParameterAs<std::size_t>(context.parameters, "count").value_or(10)};
	size = std::min(size, std::size_t {500});

	std::size_t offset {getParameterAs<std::size_t>(context.parameters, "offset").value_or(0)};

	auto transaction {context.dbSession.createSharedTransaction()};

	auto clusterType {ClusterType::getByName(context.dbSession, genreClusterName)};
	if (!clusterType)
		throw RequestedDataNotFoundError {};

	auto cluster {clusterType->getCluster(genre)};
	if (!cluster)
		throw RequestedDataNotFoundError {};

	User::pointer user {User::getById(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	Response response {Response::createOkResponse(context.serverProtocolVersion)};
	Response::Node& songsByGenreNode {response.createNode("songsByGenre")};

	bool more;
	auto tracks {Track::getByFilter(context.dbSession, {cluster->getId()}, {}, Range {offset, size}, more)};
	for (const Track::pointer& track : tracks)
		songsByGenreNode.addArrayChild("song", trackToResponseNode(track, context.dbSession, user));

	return response;
}

static
Response
handleGetUserRequest(RequestContext& context)
{
	std::string username {getMandatoryParameterAs<std::string>(context.parameters, "username")};

	auto transaction {context.dbSession.createSharedTransaction()};

	checkUserIsMySelfOrAdmin(context, username);

	const User::pointer user {User::getByLoginName(context.dbSession, username)};
	if (!user)
		throw RequestedDataNotFoundError {};

	Response response {Response::createOkResponse(context.serverProtocolVersion)};
	response.addNode("user", userToResponseNode(user));

	return response;
}

static
Response
handleGetUsersRequest(RequestContext& context)
{
	auto transaction {context.dbSession.createSharedTransaction()};

	Response response {Response::createOkResponse(context.serverProtocolVersion)};
	Response::Node& usersNode {response.createNode("users")};

	const auto users {User::getAll(context.dbSession)};
	for (const User::pointer& user : users)
		usersNode.addArrayChild("user", userToResponseNode(user));

	return response;
}

static
Response
handleSearchRequestCommon(RequestContext& context, bool id3)
{
	// Mandatory params
	std::string query {getMandatoryParameterAs<std::string>(context.parameters, "query")};

	std::vector<std::string_view> keywords {StringUtils::splitString(query, " ")};

	// Optional params
	std::size_t artistCount {getParameterAs<std::size_t>(context.parameters, "artistCount").value_or(20)};
	std::size_t artistOffset {getParameterAs<std::size_t>(context.parameters, "artistOffset").value_or(0)};
	std::size_t albumCount {getParameterAs<std::size_t>(context.parameters, "albumCount").value_or(20)};
	std::size_t albumOffset {getParameterAs<std::size_t>(context.parameters, "albumOffset").value_or(0)};
	std::size_t songCount {getParameterAs<std::size_t>(context.parameters, "songCount").value_or(20)};
	std::size_t songOffset {getParameterAs<std::size_t>(context.parameters, "songOffset").value_or(0)};

	auto transaction {context.dbSession.createSharedTransaction()};

	User::pointer user {User::getById(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	Response response {Response::createOkResponse(context.serverProtocolVersion)};
	Response::Node& searchResult2Node {response.createNode(id3 ? "searchResult3" : "searchResult2")};

	bool more;
	{
		auto artists {Artist::getByFilter(context.dbSession, {}, keywords, std::nullopt, Artist::SortMethod::BySortName, Range {artistOffset, artistCount}, more)};
		for (const Artist::pointer& artist : artists)
			searchResult2Node.addArrayChild("artist", artistToResponseNode(user, artist, id3));
	}

	{
		auto releases {Release::getByFilter(context.dbSession, {}, keywords, Range {albumOffset, albumCount}, more)};
		for (const Release::pointer& release : releases)
			searchResult2Node.addArrayChild("album", releaseToResponseNode(release, context.dbSession, user, id3));
	}

	{
		auto tracks {Track::getByFilter(context.dbSession, {}, keywords, Range {songOffset, songCount}, more)};
		for (const Track::pointer& track : tracks)
			searchResult2Node.addArrayChild("song", trackToResponseNode(track, context.dbSession, user));
	}

	return response;
}

struct StarParameters
{
	std::vector<ArtistId> artistIds;
	std::vector<ReleaseId> releaseIds;
	std::vector<TrackId> trackIds;
};

static
StarParameters
getStarParameters(const Wt::Http::ParameterMap& parameters)
{
	StarParameters res;

	// TODO handle parameters for legacy file browsing
	res.trackIds = getMultiParametersAs<TrackId>(parameters, "id");
	res.artistIds = getMultiParametersAs<ArtistId>(parameters, "artistId");
	res.releaseIds = getMultiParametersAs<ReleaseId>(parameters, "albumId");

	return res;
}

static
Response
handleStarRequest(RequestContext& context)
{
	StarParameters params {getStarParameters(context.parameters)};

	auto transaction {context.dbSession.createSharedTransaction()};

	User::pointer user {User::getById(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	for (const ArtistId id : params.artistIds)
	{
		Artist::pointer artist {Artist::getById(context.dbSession, id)};
		if (!artist)
			continue;

		user.modify()->starArtist(artist);
	}

	for (const ReleaseId id : params.releaseIds)
	{
		Release::pointer release {Release::getById(context.dbSession, id)};
		if (!release)
			continue;

		user.modify()->starRelease(release);
	}

	for (const TrackId id : params.trackIds)
	{
		Track::pointer track {Track::getById(context.dbSession, id)};
		if (!track)
			continue;

		user.modify()->starTrack(track);
	}

	return Response::createOkResponse(context.serverProtocolVersion);
}

static
Response
handleSearch2Request(RequestContext& context)
{
	return handleSearchRequestCommon(context, false /* no id3 */);
}

static
Response
handleSearch3Request(RequestContext& context)
{
	return handleSearchRequestCommon(context, true /* id3 */);
}

static
Response
handleUnstarRequest(RequestContext& context)
{
	StarParameters params {getStarParameters(context.parameters)};

	auto transaction {context.dbSession.createSharedTransaction()};

	User::pointer user {User::getById(context.dbSession, context.userId)};
	if (!user)
		throw RequestedDataNotFoundError {};

	for (const ArtistId id : params.artistIds)
	{
		Artist::pointer artist {Artist::getById(context.dbSession, id)};
		if (!artist)
			continue;

		user.modify()->unstarArtist(artist);
	}

	for (const ReleaseId id : params.releaseIds)
	{
		Release::pointer release {Release::getById(context.dbSession, id)};
		if (!release)
			continue;

		user.modify()->unstarRelease(release);
	}

	for (const TrackId id : params.trackIds)
	{
		Track::pointer track {Track::getById(context.dbSession, id)};
		if (!track)
			continue;

		user.modify()->unstarTrack(track);
	}


	return Response::createOkResponse(context.serverProtocolVersion);
}

static
Response
handleScrobble(RequestContext& context)
{
	const std::vector<TrackId> ids {getMandatoryMultiParametersAs<TrackId>(context.parameters, "id")};
	const std::vector<unsigned long> times {getMultiParametersAs<unsigned long>(context.parameters, "time")};
	const bool submission{getParameterAs<bool>(context.parameters, "submission").value_or(true)};

	// playing now => no time to be provided
	if (!submission && !times.empty())
		throw BadParameterGenericError {"time"};

	// playing now => only one at a time
	if (!submission && ids.size() > 1)
		throw BadParameterGenericError {"id"};

	// if multiple submissions, must have times
	if (ids.size() > 1 && ids.size() != times.size())
		throw BadParameterGenericError {"time"};

	if (!submission)
	{
		Service<Scrobbling::IScrobbling>::get()->listenStarted({context.userId, ids.front()});
	}
	else
	{
		if (times.empty())
		{
			Service<Scrobbling::IScrobbling>::get()->listenFinished({context.userId, ids.front()});
		}
		else
		{
			for (std::size_t i {}; i < ids.size(); ++i)
			{
				const TrackId trackId {ids[i]};
				const unsigned long time {times[i]};
				Service<Scrobbling::IScrobbling>::get()->addTimedListen({{context.userId, trackId}, Wt::WDateTime::fromTime_t(static_cast<std::time_t>(time / 1000))});
			}
		}
	}

	return Response::createOkResponse(context.serverProtocolVersion);
}

static
Response
handleUpdateUserRequest(RequestContext& context)
{
	std::string username {getMandatoryParameterAs<std::string>(context.parameters, "username")};
	std::optional<std::string> password {getParameterAs<std::string>(context.parameters, "password")};

	UserId userId;
	{
		auto transaction {context.dbSession.createSharedTransaction()};

		User::pointer user {User::getByLoginName(context.dbSession, username)};
		if (!user)
			throw RequestedDataNotFoundError {};

		userId = user->getId();
	}

	if (password)
	{
		checkSetPasswordImplemented();

		try
		{
			Service<::Auth::IPasswordService>()->setPassword(userId, decodePasswordIfNeeded(*password));
		}
		catch (const Auth::PasswordMustMatchLoginNameException&)
		{
			throw PasswordMustMatchLoginNameGenericError {};
		}
		catch (const Auth::PasswordTooWeakException&)
		{
			throw PasswordTooWeakGenericError {};
		}
		catch (const Auth::Exception&)
		{
			throw UserNotAuthorizedError {};
		}
	}

	return Response::createOkResponse(context.serverProtocolVersion);
}

static
Response
handleUpdatePlaylistRequest(RequestContext& context)
{
	// Mandatory params
	TrackListId id {getMandatoryParameterAs<TrackListId>(context.parameters, "playlistId")};

	// Optional parameters
	auto name {getParameterAs<std::string>(context.parameters, "name")};
	auto isPublic {getParameterAs<bool>(context.parameters, "public")};

	std::vector<TrackId> trackIdsToAdd {getMultiParametersAs<TrackId>(context.parameters, "songIdToAdd")};
	std::vector<std::size_t> trackPositionsToRemove {getMultiParametersAs<std::size_t>(context.parameters, "songIndexToRemove")};

	auto transaction {context.dbSession.createUniqueTransaction()};

	User::pointer user {User::getById(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	TrackList::pointer tracklist {TrackList::getById(context.dbSession, id)};
	if (!tracklist
		|| tracklist->getUser() != user
		|| tracklist->getType() != TrackList::Type::Playlist)
	{
		throw RequestedDataNotFoundError {};
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
	for (const TrackId trackIdToAdd : trackIdsToAdd)
	{
		Track::pointer track {Track::getById(context.dbSession, trackIdToAdd)};
		if (!track)
			continue;

		TrackListEntry::create(context.dbSession, track, tracklist);
	}

	return Response::createOkResponse(context.serverProtocolVersion);
}

static
Response
handleGetBookmarks(RequestContext& context)
{
	auto transaction {context.dbSession.createSharedTransaction()};

	User::pointer user {User::getById(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	const auto bookmarks {TrackBookmark::getByUser(context.dbSession, user)};

	Response response {Response::createOkResponse(context.serverProtocolVersion)};
	Response::Node& bookmarksNode {response.createNode("bookmarks")};

	for (const TrackBookmark::pointer& bookmark : bookmarks)
	{
		Response::Node bookmarkNode {trackBookmarkToResponseNode(bookmark)};
		bookmarkNode.addArrayChild("entry", trackToResponseNode(bookmark->getTrack(), context.dbSession, user));

		bookmarksNode.addArrayChild("bookmark", std::move(bookmarkNode));
	}

	return response ;
}

static
Response
handleCreateBookmark(RequestContext& context)
{
	// Mandatory params
	TrackId id {getMandatoryParameterAs<TrackId>(context.parameters, "id")};
	unsigned long position {getMandatoryParameterAs<unsigned long>(context.parameters, "position")};
	const std::optional<std::string> comment {getParameterAs<std::string>(context.parameters, "comment")};

	auto transaction {context.dbSession.createUniqueTransaction()};

	const User::pointer user {User::getById(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	const Track::pointer track {Track::getById(context.dbSession, id)};
	if (!track)
		throw RequestedDataNotFoundError {};

	// Replace any existing bookmark
	auto bookmark {TrackBookmark::getByUser(context.dbSession, user, track)};
	if (!bookmark)
		bookmark = TrackBookmark::create(context.dbSession, user, track);

	bookmark.modify()->setOffset(std::chrono::milliseconds {position});
	if (comment)
		bookmark.modify()->setComment(*comment);

	return Response::createOkResponse(context.serverProtocolVersion);
}

static
Response
handleDeleteBookmark(RequestContext& context)
{
	// Mandatory params
	TrackId id {getMandatoryParameterAs<TrackId>(context.parameters, "id")};

	auto transaction {context.dbSession.createUniqueTransaction()};

	const User::pointer user {User::getById(context.dbSession, context.userId)};
	if (!user)
		throw UserNotAuthorizedError {};

	const Track::pointer track {Track::getById(context.dbSession, id)};
	if (!track)
		throw RequestedDataNotFoundError {};

	auto bookmark {TrackBookmark::getByUser(context.dbSession, user, track)};
	if (!bookmark)
		throw RequestedDataNotFoundError {};

	bookmark.remove();

	return Response::createOkResponse(context.serverProtocolVersion);
}

static
Response
handleNotImplemented(RequestContext&)
{
	throw NotImplementedGenericError {};
}

static
void
handleGetCoverArt(RequestContext& context, const Wt::Http::Request& /*request*/, Wt::Http::Response& response)
{
	// Mandatory params
	const auto trackId {getParameterAs<TrackId>(context.parameters, "id")};
	const auto releaseId {getParameterAs<ReleaseId>(context.parameters, "id")};

	if (!trackId && !releaseId)
		throw BadParameterGenericError {"id"};

	std::size_t size {getParameterAs<std::size_t>(context.parameters, "size").value_or(256)};
	size = Utils::clamp(size, std::size_t {32}, std::size_t {1024});

	std::shared_ptr<Image::IEncodedImage> cover;
	if (trackId)
		cover = Service<Cover::ICoverService>::get()->getFromTrack(*trackId, size);
	else if (releaseId)
		cover = Service<Cover::ICoverService>::get()->getFromRelease(*releaseId, size);

	response.out().write(reinterpret_cast<const char*>(cover->getData()), cover->getDataSize());
	response.setMimeType(std::string {cover->getMimeType()});
}

using RequestHandlerFunc = std::function<Response(RequestContext& context)>;
using CheckImplementedFunc = std::function<void()>;
struct RequestEntryPointInfo
{
	RequestHandlerFunc			func;
	EnumSet<UserType>			allowedUserTypes {UserType::DEMO, UserType::REGULAR, UserType::ADMIN};
	CheckImplementedFunc		checkFunc {};
};

static const std::unordered_map<std::string, RequestEntryPointInfo> requestEntryPoints
{
	// System
	{"ping",		{handlePingRequest}},
	{"getLicense",		{handleGetLicenseRequest}},

	// Browsing
	{"getMusicFolders",	{handleGetMusicFoldersRequest}},
	{"getIndexes",		{handleGetIndexesRequest}},
	{"getMusicDirectory",	{handleGetMusicDirectoryRequest}},
	{"getGenres",		{handleGetGenresRequest}},
	{"getArtists",		{handleGetArtistsRequest}},
	{"getArtist",		{handleGetArtistRequest}},
	{"getAlbum",		{handleGetAlbumRequest}},
	{"getSong",		{handleNotImplemented}},
	{"getVideos",		{handleNotImplemented}},
	{"getArtistInfo",	{handleGetArtistInfoRequest}},
	{"getArtistInfo2",	{handleGetArtistInfo2Request}},
	{"getAlbumInfo",	{handleNotImplemented}},
	{"getAlbumInfo2",	{handleNotImplemented}},
	{"getSimilarSongs",	{handleGetSimilarSongsRequest}},
	{"getSimilarSongs2",	{handleGetSimilarSongs2Request}},
	{"getTopSongs",		{handleNotImplemented}},

	// Album/song lists
	{"getAlbumList",	{handleGetAlbumListRequest}},
	{"getAlbumList2",	{handleGetAlbumList2Request}},
	{"getRandomSongs",	{handleGetRandomSongsRequest}},
	{"getSongsByGenre",	{handleGetSongsByGenreRequest}},
	{"getNowPlaying",	{handleNotImplemented}},
	{"getStarred",		{handleGetStarredRequest}},
	{"getStarred2",		{handleGetStarred2Request}},

	// Searching
	{"search",		{handleNotImplemented}},
	{"search2",		{handleSearch2Request}},
	{"search3",		{handleSearch3Request}},

	// Playlists
	{"getPlaylists",	{handleGetPlaylistsRequest}},
	{"getPlaylist",		{handleGetPlaylistRequest}},
	{"createPlaylist",	{handleCreatePlaylistRequest}},
	{"updatePlaylist",	{handleUpdatePlaylistRequest}},
	{"deletePlaylist",	{handleDeletePlaylistRequest}},

	// Media retrieval
	{"hls",				{handleNotImplemented}},
	{"getCaptions",		{handleNotImplemented}},
	{"getLyrics",		{handleNotImplemented}},
	{"getAvatar",		{handleNotImplemented}},

	// Media annotation
	{"star",			{handleStarRequest}},
	{"unstar",			{handleUnstarRequest}},
	{"setRating",		{handleNotImplemented}},
	{"scrobble",		{handleScrobble}},

	// Sharing
	{"getShares",		{handleNotImplemented}},
	{"createShares",	{handleNotImplemented}},
	{"updateShare",		{handleNotImplemented}},
	{"deleteShare",		{handleNotImplemented}},

	// Podcast
	{"getPodcasts",			{handleNotImplemented}},
	{"getNewestPodcasts",		{handleNotImplemented}},
	{"refreshPodcasts",		{handleNotImplemented}},
	{"createPodcastChannel",	{handleNotImplemented}},
	{"deletePodcastChannel",	{handleNotImplemented}},
	{"deletePodcastEpisode",	{handleNotImplemented}},
	{"downloadPodcastEpisode",	{handleNotImplemented}},

	// Jukebox
	{"jukeboxControl",	{handleNotImplemented}},

	// Internet radio
	{"getInternetRadioStations",	{handleNotImplemented}},
	{"createInternetRadioStation",	{handleNotImplemented}},
	{"updateInternetRadioStation",	{handleNotImplemented}},
	{"deleteInternetRadioStation",	{handleNotImplemented}},

	// Chat
	{"getChatMessages",	{handleNotImplemented}},
	{"addChatMessages",	{handleNotImplemented}},

	// User management
	{"getUser",			{handleGetUserRequest}},
	{"getUsers",		{handleGetUsersRequest,			{UserType::ADMIN}}},
	{"createUser",		{handleCreateUserRequest,		{UserType::ADMIN}, &checkSetPasswordImplemented}},
	{"updateUser",		{handleUpdateUserRequest,		{UserType::ADMIN}}},
	{"deleteUser",		{handleDeleteUserRequest,		{UserType::ADMIN}}},
	{"changePassword",	{handleChangePassword,			{UserType::REGULAR, UserType::ADMIN}, &checkSetPasswordImplemented}},

	// Bookmarks
	{"getBookmarks",	{handleGetBookmarks}},
	{"createBookmark",	{handleCreateBookmark}},
	{"deleteBookmark",	{handleDeleteBookmark}},
	{"getPlayQueue",	{handleNotImplemented}},
	{"savePlayQueue",	{handleNotImplemented}},

	// Media library scanning
	{"getScanStatus",	{Scan::handleGetScanStatus,		{UserType::ADMIN}}},
	{"startScan",		{Scan::handleStartScan,			{UserType::ADMIN}}},
};

using MediaRetrievalHandlerFunc = std::function<void(RequestContext&, const Wt::Http::Request&, Wt::Http::Response&)>;
static std::unordered_map<std::string, MediaRetrievalHandlerFunc> mediaRetrievalHandlers
{
	// Media retrieval
	{"download",		Stream::handleDownload},
	{"stream",			Stream::handleStream},
	{"getCoverArt",		handleGetCoverArt},
};


void
SubsonicResource::handleRequest(const Wt::Http::Request &request, Wt::Http::Response &response)
{
	static std::atomic<std::size_t> curRequestId {};

	const std::size_t requestId {curRequestId++};

	LMS_LOG(API_SUBSONIC, DEBUG) << "Handling request " << requestId << " '" << request.pathInfo() << "', continuation = " << (request.continuation() ? "true" : "false") << ", params = " << parameterMapToDebugString(request.getParameterMap());

	std::string requestPath {request.pathInfo()};
	if (StringUtils::stringEndsWith(requestPath, ".view"))
		requestPath.resize(requestPath.length() - 5);

	// Optional parameters
	const ResponseFormat format {getParameterAs<std::string>(request.getParameterMap(), "f").value_or("xml") == "json" ? ResponseFormat::json : ResponseFormat::xml};

	ProtocolVersion protocolVersion {defaultServerProtocolVersion};

	try
	{
		// We need to parse client a soon as possible to make sure to answer with the right protocol version
		protocolVersion = getServerProtocolVersion(getMandatoryParameterAs<std::string>(request.getParameterMap(), "c"));
		RequestContext requestContext {buildRequestContext(request)};

		auto itEntryPoint {requestEntryPoints.find(requestPath)};
		if (itEntryPoint != requestEntryPoints.end())
		{
			if (itEntryPoint->second.checkFunc)
				itEntryPoint->second.checkFunc();

			checkUserTypeIsAllowed(requestContext, itEntryPoint->second.allowedUserTypes);

			Response resp {(itEntryPoint->second.func)(requestContext)};

			resp.write(response.out(), format);
			response.setMimeType(ResponseFormatToMimeType(format));

			LMS_LOG(API_SUBSONIC, DEBUG) << "Request " << requestId << " '" << requestPath << "' handled!";
			return;
		}

		auto itStreamHandler {mediaRetrievalHandlers.find(requestPath)};
		if (itStreamHandler != mediaRetrievalHandlers.end())
		{
			itStreamHandler->second(requestContext, request, response);
			LMS_LOG(API_SUBSONIC, DEBUG) << "Request " << requestId  << " '" << requestPath << "' handled!";
			return;
		}

		LMS_LOG(API_SUBSONIC, ERROR) << "Unhandled command '" << requestPath << "'";
		throw UnknownEntryPointGenericError {};
	}
	catch (const Error& e)
	{
		LMS_LOG(API_SUBSONIC, ERROR) << "Error while processing request '" << requestPath << "'"
			<< ", params = [" << parameterMapToDebugString(request.getParameterMap()) << "]"
			<< ", code = " << static_cast<int>(e.getCode()) << ", msg = '" << e.getMessage() << "'";
		Response resp {Response::createFailedResponse(protocolVersion, e)};
		resp.write(response.out(), format);
		response.setMimeType(ResponseFormatToMimeType(format));
	}
}

ProtocolVersion
SubsonicResource::getServerProtocolVersion(const std::string& clientName) const
{
	auto it {_serverProtocolVersionsByClient.find(clientName)};
	if (it == std::cend(_serverProtocolVersionsByClient))
		return defaultServerProtocolVersion;

	return it->second;
}

void
SubsonicResource::checkProtocolVersion(ProtocolVersion client, ProtocolVersion server)
{
	if (client.major > server.major)
		throw ServerMustUpgradeError {};
	if (client.major < server.major)
		throw ClientMustUpgradeError {};
	if (client.minor > server.minor)
		throw ServerMustUpgradeError {};
	else if (client.minor == server.minor)
	{
		if (client.patch > server.patch)
			throw ServerMustUpgradeError {};
	}
}

ClientInfo
SubsonicResource::getClientInfo(const Wt::Http::ParameterMap& parameters)
{
	ClientInfo res;

	// Mandatory parameters
	res.name = getMandatoryParameterAs<std::string>(parameters, "c");
	res.version = getMandatoryParameterAs<ProtocolVersion>(parameters, "v");
	res.user = getMandatoryParameterAs<std::string>(parameters, "u");
	res.password = decodePasswordIfNeeded(getMandatoryParameterAs<std::string>(parameters, "p"));

	return res;
}

RequestContext
SubsonicResource::buildRequestContext(const Wt::Http::Request& request)
{
	const Wt::Http::ParameterMap& parameters {request.getParameterMap()};
	const ClientInfo clientInfo {getClientInfo(parameters)};
	const Database::UserId userId {authenticateUser(request, clientInfo)};

	return {parameters, _db.getTLSSession(), userId, clientInfo, getServerProtocolVersion(clientInfo.name)};
}

Database::UserId
SubsonicResource::authenticateUser(const Wt::Http::Request& request, const ClientInfo& clientInfo)
{
	if (auto *authEnvService {Service<::Auth::IEnvService>::get()})
	{
		const auto checkResult {authEnvService->processRequest(request)};
		if (checkResult.state != ::Auth::IEnvService::CheckResult::State::Granted)
			throw UserNotAuthorizedError {};

		return *checkResult.userId;
	}
	else if (auto *authPasswordService {Service<::Auth::IPasswordService>::get()})
	{
		const auto checkResult {authPasswordService->checkUserPassword(boost::asio::ip::address::from_string(request.clientAddress()),
																			clientInfo.user, clientInfo.password)};

		switch (checkResult.state)
		{
			case Auth::IPasswordService::CheckResult::State::Granted:
				return *checkResult.userId;
				break;
			case Auth::IPasswordService::CheckResult::State::Denied:
				throw WrongUsernameOrPasswordError {};
			case Auth::IPasswordService::CheckResult::State::Throttled:
				throw LoginThrottledGenericError {};
		}
	}

	throw InternalErrorGenericError {"No service avalaible to authenticate user"};
}

} // namespace api::subsonic

