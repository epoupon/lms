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

#include "Browsing.hpp"

#include "database/Artist.hpp"
#include "database/Cluster.hpp"
#include "database/Session.hpp"
#include "database/Release.hpp"
#include "database/Track.hpp"
#include "database/User.hpp"
#include "services/recommendation/IRecommendationService.hpp"
#include "utils/ILogger.hpp"
#include "utils/Random.hpp"
#include "utils/Service.hpp"
#include "responses/Album.hpp"
#include "responses/Artist.hpp"
#include "responses/Genre.hpp"
#include "responses/Song.hpp"
#include "ParameterParsing.hpp"
#include "SubsonicId.hpp"
#include "Utils.hpp"

namespace API::Subsonic
{
    using namespace Database;

    static const std::string_view	reportedDummyDate{ "2000-01-01T00:00:00" };
    static const unsigned long long	reportedDummyDateULong{ 946684800000ULL }; // 2000-01-01T00:00:00 UTC

    namespace
    {
        Response handleGetArtistInfoRequestCommon(RequestContext& context, bool id3)
        {
            // Mandatory params
            ArtistId id{ getMandatoryParameterAs<ArtistId>(context.parameters, "id") };

            // Optional params
            std::size_t count{ getParameterAs<std::size_t>(context.parameters, "count").value_or(20) };

            Response response{ Response::createOkResponse(context.serverProtocolVersion) };
            Response::Node& artistInfoNode{ response.createNode(id3 ? Response::Node::Key{ "artistInfo2" } : Response::Node::Key{ "artistInfo" }) };

            {
                auto transaction{ context.dbSession.createReadTransaction() };

                const Artist::pointer artist{ Artist::find(context.dbSession, id) };
                if (!artist)
                    throw RequestedDataNotFoundError{};

                std::optional<UUID> artistMBID{ artist->getMBID() };
                if (artistMBID)
                    artistInfoNode.createChild("musicBrainzId").setValue(artistMBID->getAsString());
            }

            auto similarArtistsId{ Service<Recommendation::IRecommendationService>::get()->getSimilarArtists(id, {TrackArtistLinkType::Artist, TrackArtistLinkType::ReleaseArtist}, count) };

            {
                auto transaction{ context.dbSession.createReadTransaction() };

                User::pointer user{ User::find(context.dbSession, context.userId) };
                if (!user)
                    throw UserNotAuthorizedError{};

                for (const ArtistId similarArtistId : similarArtistsId)
                {
                    const Artist::pointer similarArtist{ Artist::find(context.dbSession, similarArtistId) };
                    if (similarArtist)
                        artistInfoNode.addArrayChild("similarArtist", createArtistNode(context, similarArtist, user, id3));
                }
            }

            return response;
        }

        Response handleGetArtistsRequestCommon(RequestContext& context, bool id3)
        {
            Response response{ Response::createOkResponse(context.serverProtocolVersion) };

            Response::Node& artistsNode{ response.createNode(id3 ? "artists" : "indexes") };
            artistsNode.setAttribute("ignoredArticles", "");
            artistsNode.setAttribute("lastModified", reportedDummyDateULong); // TODO report last file write?

            Artist::FindParameters parameters;
            {
                auto transaction{ context.dbSession.createReadTransaction() };

                User::pointer user{ User::find(context.dbSession, context.userId) };
                if (!user)
                    throw UserNotAuthorizedError{};

                parameters.setSortMethod(ArtistSortMethod::BySortName);
                switch (user->getSubsonicArtistListMode())
                {
                case SubsonicArtistListMode::AllArtists:
                    break;
                case SubsonicArtistListMode::ReleaseArtists:
                    parameters.setLinkType(TrackArtistLinkType::ReleaseArtist);
                    break;
                case SubsonicArtistListMode::TrackArtists:
                    parameters.setLinkType(TrackArtistLinkType::Artist);
                    break;
                }
            }

            // This endpoint does not scale: make sort lived transactions in order not to block the whole application

            // first pass: dispatch the artists by first letter
            LMS_LOG(API_SUBSONIC, DEBUG, "GetArtists: fetching all artists...");
            std::map<char, std::vector<ArtistId>> artistsSortedByFirstChar;
            std::size_t currentArtistOffset{ 0 };
            constexpr std::size_t batchSize{ 100 };
            bool hasMoreArtists{ true };
            while (hasMoreArtists)
            {
                auto transaction{ context.dbSession.createReadTransaction() };

                parameters.setRange(Range{ currentArtistOffset, batchSize });
                const auto artists{ Artist::find(context.dbSession, parameters) };
                for (const Artist::pointer& artist : artists.results)
                {
                    std::string_view sortName{ artist->getSortName() };

                    char sortChar;
                    if (sortName.empty() || !std::isalpha(sortName[0]))
                        sortChar = '?';
                    else
                        sortChar = std::toupper(sortName[0]);

                    artistsSortedByFirstChar[sortChar].push_back(artist->getId());
                }

                hasMoreArtists = artists.moreResults;
                currentArtistOffset += artists.results.size();
            }

            // second pass: add each artist
            LMS_LOG(API_SUBSONIC, DEBUG, "GetArtists: constructing response...");
            for (const auto& [sortChar, artistIds] : artistsSortedByFirstChar)
            {
                Response::Node& indexNode{ artistsNode.createArrayChild("index") };
                indexNode.setAttribute("name", std::string{ sortChar });

                for (const ArtistId artistId : artistIds)
                {
                    auto transaction{ context.dbSession.createReadTransaction() };
                    User::pointer user{ User::find(context.dbSession, context.userId) };
                    if (!user)
                        throw UserNotAuthorizedError{};

                    if (const Artist::pointer artist{ Artist::find(context.dbSession, artistId) })
                        indexNode.addArrayChild("artist", createArtistNode(context, artist, user, id3));
                }
            }

            return response;
        }

        std::vector<TrackId> findSimilarSongs(RequestContext& context, ArtistId artistId, std::size_t count)
        {
            // API says: "Returns a random collection of songs from the given artist and similar artists"
            const std::size_t similarArtistCount{ count / 5 };
            std::vector<ArtistId> artistIds{ Service<Recommendation::IRecommendationService>::get()->getSimilarArtists(artistId, {TrackArtistLinkType::Artist, TrackArtistLinkType::ReleaseArtist}, similarArtistCount) };
            artistIds.push_back(artistId);

            const std::size_t meanTrackCountPerArtist{ (count / artistIds.size()) + 1 };

            auto transaction{ context.dbSession.createReadTransaction() };

            std::vector<TrackId> tracks;
            tracks.reserve(count);

            for (const ArtistId id : artistIds)
            {
                Track::FindParameters params;
                params.setArtist(id);
                params.setRange(Range{ 0, meanTrackCountPerArtist });
                params.setSortMethod(TrackSortMethod::Random);

                const auto artistTracks{ Track::findIds(context.dbSession, params) };
                tracks.insert(std::end(tracks),
                    std::begin(artistTracks.results),
                    std::end(artistTracks.results));
            }

            return tracks;
        }

        std::vector<TrackId> findSimilarSongs(RequestContext& context, ReleaseId releaseId, std::size_t count)
        {
            // API says: "Returns a random collection of songs from the given artist and similar artists"
            // so let's extend this for release
            const std::size_t similarReleaseCount{ count / 5 };
            std::vector<ReleaseId> releaseIds{ Service<Recommendation::IRecommendationService>::get()->getSimilarReleases(releaseId, similarReleaseCount) };
            releaseIds.push_back(releaseId);

            const std::size_t meanTrackCountPerRelease{ (count / releaseIds.size()) + 1 };

            auto transaction{ context.dbSession.createReadTransaction() };

            std::vector<TrackId> tracks;
            tracks.reserve(count);

            for (const ReleaseId id : releaseIds)
            {
                Track::FindParameters params;
                params.setRelease(id);
                params.setRange(Range{ 0, meanTrackCountPerRelease });
                params.setSortMethod(TrackSortMethod::Random);

                const auto releaseTracks{ Track::findIds(context.dbSession, params) };
                tracks.insert(std::end(tracks),
                    std::begin(releaseTracks.results),
                    std::end(releaseTracks.results));
            }

            return tracks;
        }

        std::vector<TrackId> findSimilarSongs(RequestContext&, TrackId trackId, std::size_t count)
        {
            return Service<Recommendation::IRecommendationService>::get()->findSimilarTracks({ trackId }, count);
        }

        Response handleGetSimilarSongsRequestCommon(RequestContext& context, bool id3)
        {
            // Optional params
            std::size_t count{ getParameterAs<std::size_t>(context.parameters, "count").value_or(50) };
            if (count > defaultMaxCountSize)
                throw ParameterValueTooHighGenericError{ "count", defaultMaxCountSize };

            std::vector<TrackId> tracks;

            if (const auto artistId{ getParameterAs<ArtistId>(context.parameters, "id") })
                tracks = findSimilarSongs(context, *artistId, count);
            else if (const auto releaseId{ getParameterAs<ReleaseId>(context.parameters, "id") })
                tracks = findSimilarSongs(context, *releaseId, count);
            else if (const auto trackId{ getParameterAs<TrackId>(context.parameters, "id") })
                tracks = findSimilarSongs(context, *trackId, count);
            else
                throw BadParameterGenericError{ "id" };

            Random::shuffleContainer(tracks);

            auto transaction{ context.dbSession.createReadTransaction() };

            User::pointer user{ User::find(context.dbSession, context.userId) };
            if (!user)
                throw UserNotAuthorizedError{};

            Response response{ Response::createOkResponse(context.serverProtocolVersion) };
            Response::Node& similarSongsNode{ response.createNode(id3 ? Response::Node::Key{ "similarSongs2" } : Response::Node::Key{ "similarSongs" }) };
            for (const TrackId trackId : tracks)
            {
                const Track::pointer track{ Track::find(context.dbSession, trackId) };
                similarSongsNode.addArrayChild("song", createSongNode(context, track, user));
            }

            return response;
        }

    }

    Response handleGetMusicFoldersRequest(RequestContext& context)
    {
        Response response{ Response::createOkResponse(context.serverProtocolVersion) };
        Response::Node& musicFoldersNode{ response.createNode("musicFolders") };

        Response::Node& musicFolderNode{ musicFoldersNode.createArrayChild("musicFolder") };
        musicFolderNode.setAttribute("id", "0");
        musicFolderNode.setAttribute("name", "Music");

        return response;
    }

    Response handleGetIndexesRequest(RequestContext& context)
    {
        return handleGetArtistsRequestCommon(context, false /* no id3 */);
    }

    Response handleGetMusicDirectoryRequest(RequestContext& context)
    {
        // Mandatory params
        const auto artistId{ getParameterAs<ArtistId>(context.parameters, "id") };
        const auto releaseId{ getParameterAs<ReleaseId>(context.parameters, "id") };
        const auto root{ getParameterAs<RootId>(context.parameters, "id") };

        if (!root && !artistId && !releaseId)
            throw BadParameterGenericError{ "id" };

        Response response{ Response::createOkResponse(context.serverProtocolVersion) };
        Response::Node& directoryNode{ response.createNode("directory") };

        auto transaction{ context.dbSession.createReadTransaction() };

        User::pointer user{ User::find(context.dbSession, context.userId) };
        if (!user)
            throw UserNotAuthorizedError{};

        if (root)
        {
            directoryNode.setAttribute("id", idToString(RootId{}));
            directoryNode.setAttribute("name", "Music");

            // TODO: this does not scale when a lot of artists are present
            Artist::find(context.dbSession, Artist::FindParameters{}.setSortMethod(ArtistSortMethod::BySortName), [&](const Artist::pointer& artist)
                {
                    directoryNode.addArrayChild("child", createArtistNode(context, artist, user, false /* no id3 */));
                });
        }
        else if (artistId)
        {
            directoryNode.setAttribute("id", idToString(*artistId));

            auto artist{ Artist::find(context.dbSession, *artistId) };
            if (!artist)
                throw RequestedDataNotFoundError{};

            directoryNode.setAttribute("name", Utils::makeNameFilesystemCompatible(artist->getName()));

            Release::find(context.dbSession, Release::FindParameters{}.setArtist(*artistId), [&](const Release::pointer& release)
                {
                    directoryNode.addArrayChild("child", createAlbumNode(context, release, user, false /* no id3 */));
                });
        }
        else if (releaseId)
        {
            directoryNode.setAttribute("id", idToString(*releaseId));

            auto release{ Release::find(context.dbSession, *releaseId) };
            if (!release)
                throw RequestedDataNotFoundError{};

            directoryNode.setAttribute("name", Utils::makeNameFilesystemCompatible(release->getName()));

            Track::find(context.dbSession, Track::FindParameters{}.setRelease(*releaseId).setSortMethod(TrackSortMethod::Release), [&](const Track::pointer& track)
                {
                    directoryNode.addArrayChild("child", createSongNode(context, track, user));
                });
        }
        else
            throw BadParameterGenericError{ "id" };

        return response;
    }

    Response handleGetGenresRequest(RequestContext& context)
    {
        Response response{ Response::createOkResponse(context.serverProtocolVersion) };

        Response::Node& genresNode{ response.createNode("genres") };

        auto transaction{ context.dbSession.createReadTransaction() };

        const ClusterType::pointer clusterType{ ClusterType::find(context.dbSession, "GENRE") };
        if (clusterType)
        {
            const auto clusters{ clusterType->getClusters() };

            for (const Cluster::pointer& cluster : clusters)
                genresNode.addArrayChild("genre", createGenreNode(cluster));
        }

        return response;
    }

    Response handleGetArtistsRequest(RequestContext& context)
    {
        return handleGetArtistsRequestCommon(context, true /* id3 */);
    }

    Response handleGetArtistRequest(RequestContext& context)
    {
        // Mandatory params
        ArtistId id{ getMandatoryParameterAs<ArtistId>(context.parameters, "id") };

        auto transaction{ context.dbSession.createReadTransaction() };

        const Artist::pointer artist{ Artist::find(context.dbSession, id) };
        if (!artist)
            throw RequestedDataNotFoundError{};

        User::pointer user{ User::find(context.dbSession, context.userId) };
        if (!user)
            throw UserNotAuthorizedError{};

        Response response{ Response::createOkResponse(context.serverProtocolVersion) };
        Response::Node artistNode{ createArtistNode(context, artist, user, true /* id3 */) };

        const auto releases{ Release::find(context.dbSession, Release::FindParameters {}.setArtist(artist->getId())) };
        for (const Release::pointer& release : releases.results)
            artistNode.addArrayChild("album", createAlbumNode(context, release, user, true /* id3 */));

        response.addNode("artist", std::move(artistNode));

        return response;
    }

    Response handleGetAlbumRequest(RequestContext& context)
    {
        // Mandatory params
        ReleaseId id{ getMandatoryParameterAs<ReleaseId>(context.parameters, "id") };

        auto transaction{ context.dbSession.createReadTransaction() };

        Release::pointer release{ Release::find(context.dbSession, id) };
        if (!release)
            throw RequestedDataNotFoundError{};

        User::pointer user{ User::find(context.dbSession, context.userId) };
        if (!user)
            throw UserNotAuthorizedError{};

        Response response{ Response::createOkResponse(context.serverProtocolVersion) };
        Response::Node albumNode{ createAlbumNode(context, release, user, true /* id3 */) };

        const auto tracks{ Track::find(context.dbSession, Track::FindParameters {}.setRelease(id).setSortMethod(TrackSortMethod::Release)) };
        for (const Track::pointer& track : tracks.results)
            albumNode.addArrayChild("song", createSongNode(context, track, user));

        response.addNode("album", std::move(albumNode));

        return response;
    }

    Response handleGetSongRequest(RequestContext& context)
    {
        // Mandatory params
        TrackId id{ getMandatoryParameterAs<TrackId>(context.parameters, "id") };

        auto transaction{ context.dbSession.createReadTransaction() };

        const Track::pointer track{ Track::find(context.dbSession, id) };
        if (!track)
            throw RequestedDataNotFoundError{};

        User::pointer user{ User::find(context.dbSession, context.userId) };
        if (!user)
            throw UserNotAuthorizedError{};

        Response response{ Response::createOkResponse(context.serverProtocolVersion) };
        response.addNode("song", createSongNode(context, track, user));

        return response;
    }

    Response handleGetArtistInfoRequest(RequestContext& context)
    {
        return handleGetArtistInfoRequestCommon(context, false /* no id3 */);
    }

    Response handleGetArtistInfo2Request(RequestContext& context)
    {
        return handleGetArtistInfoRequestCommon(context, true /* id3 */);
    }

    Response handleGetSimilarSongsRequest(RequestContext& context)
    {
        return handleGetSimilarSongsRequestCommon(context, false /* no id3 */);
    }

    Response handleGetSimilarSongs2Request(RequestContext& context)
    {
        return handleGetSimilarSongsRequestCommon(context, true /* id3 */);
    }

}
