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

#include "core/ILogger.hpp"
#include "core/Random.hpp"
#include "core/Service.hpp"
#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/ArtistInfo.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/User.hpp"
#include "services/feedback/IFeedbackService.hpp"
#include "services/recommendation/IRecommendationService.hpp"
#include "services/scrobbling/IScrobblingService.hpp"

#include "ParameterParsing.hpp"
#include "SubsonicId.hpp"
#include "responses/Album.hpp"
#include "responses/AlbumInfo.hpp"
#include "responses/Artist.hpp"
#include "responses/Genre.hpp"
#include "responses/Song.hpp"

namespace lms::api::subsonic
{
    using namespace db;

    static const unsigned long long reportedDummyDateULong{ 946684800000ULL }; // 2000-01-01T00:00:00 UTC

    namespace
    {
        std::vector<Directory::pointer> getRootDirectories(Session& session, MediaLibraryId libraryId)
        {
            std::vector<Directory::pointer> res;

            if (libraryId.isValid())
            {
                if (const MediaLibrary::pointer library{ MediaLibrary::find(session, libraryId) })
                {
                    if (Directory::pointer rootDirectory{ Directory::find(session, library->getPath()) })
                        res.push_back(rootDirectory);
                }
            }
            else
            {
                res = Directory::findRootDirectories(session).results;
            }

            return res;
        }

        struct IndexComparator
        {
            constexpr bool operator()(char lhs, char rhs) const
            {
                if (lhs == '#' && std::isalpha(rhs))
                    return false;
                if (rhs == '#' && std::isalpha(lhs))
                    return true;

                return lhs < rhs;
            }
        };

        using IndexMap = std::map<char, std::vector<Directory::pointer>, IndexComparator>;
        void getIndexedChildDirectories(RequestContext& context, const Directory::pointer& parentDirectory, IndexMap& res)
        {
            Directory::FindParameters params;
            params.setParentDirectory(parentDirectory->getId());
            params.setSortMethod(DirectorySortMethod::Name);

            Directory::find(context.getDbSession(), params, [&](const Directory::pointer& directory) {
                const std::string_view name{ directory->getName() };
                assert(!name.empty());

                char sortChar;
                if (name.empty() || !std::isalpha(name[0]))
                    sortChar = '#';
                else
                    sortChar = std::toupper(name[0]);

                res[sortChar].push_back(directory);
            });
        }

        std::vector<TrackId> findSimilarSongs(RequestContext& context, ArtistId artistId, std::size_t count)
        {
            // API says: "Returns a random collection of songs from the given artist and similar artists"
            const std::size_t similarArtistCount{ count / 5 };
            std::vector<ArtistId> artistIds{ core::Service<recommendation::IRecommendationService>::get()->getSimilarArtists(artistId, { TrackArtistLinkType::Artist, TrackArtistLinkType::ReleaseArtist }, similarArtistCount) };
            artistIds.push_back(artistId);

            const std::size_t meanTrackCountPerArtist{ (count / artistIds.size()) + 1 };

            auto transaction{ context.getDbSession().createReadTransaction() };

            std::vector<TrackId> tracks;
            tracks.reserve(count);

            for (const ArtistId id : artistIds)
            {
                Track::FindParameters params;
                params.setArtist(id);
                params.setRange(Range{ 0, meanTrackCountPerArtist });
                params.setSortMethod(TrackSortMethod::Random);

                const auto artistTracks{ Track::findIds(context.getDbSession(), params) };
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
            std::vector<ReleaseId> releaseIds{ core::Service<recommendation::IRecommendationService>::get()->getSimilarReleases(releaseId, similarReleaseCount) };
            releaseIds.push_back(releaseId);

            const std::size_t meanTrackCountPerRelease{ (count / releaseIds.size()) + 1 };

            auto transaction{ context.getDbSession().createReadTransaction() };

            std::vector<TrackId> tracks;
            tracks.reserve(count);

            for (const ReleaseId id : releaseIds)
            {
                Track::FindParameters params;
                params.setRelease(id);
                params.setRange(Range{ 0, meanTrackCountPerRelease });
                params.setSortMethod(TrackSortMethod::Random);

                const auto releaseTracks{ Track::findIds(context.getDbSession(), params) };
                tracks.insert(std::end(tracks),
                              std::begin(releaseTracks.results),
                              std::end(releaseTracks.results));
            }

            return tracks;
        }

        std::vector<TrackId> findSimilarSongs(RequestContext& /*context*/, TrackId trackId, std::size_t count)
        {
            return core::Service<recommendation::IRecommendationService>::get()->findSimilarTracks({ trackId }, count);
        }

        Response handleGetSimilarSongsRequestCommon(RequestContext& context, bool id3)
        {
            // Optional params
            std::size_t count{ getParameterAs<std::size_t>(context.getParameters(), "count").value_or(50) };
            if (count > defaultMaxCountSize)
                throw ParameterValueTooHighGenericError{ "count", defaultMaxCountSize };

            std::vector<TrackId> tracks;

            if (const auto artistId{ getParameterAs<ArtistId>(context.getParameters(), "id") })
                tracks = findSimilarSongs(context, *artistId, count);
            else if (const auto releaseId{ getParameterAs<ReleaseId>(context.getParameters(), "id") })
                tracks = findSimilarSongs(context, *releaseId, count);
            else if (const auto trackId{ getParameterAs<TrackId>(context.getParameters(), "id") })
                tracks = findSimilarSongs(context, *trackId, count);
            else
                throw BadParameterGenericError{ "id" };

            core::random::shuffleContainer(tracks);

            auto transaction{ context.getDbSession().createReadTransaction() };

            Response response{ Response::createOkResponse(context.getServerProtocolVersion()) };
            Response::Node& similarSongsNode{ response.createNode(id3 ? Response::Node::Key{ "similarSongs2" } : Response::Node::Key{ "similarSongs" }) };
            for (const TrackId trackId : tracks)
            {
                const Track::pointer track{ Track::find(context.getDbSession(), trackId) };
                similarSongsNode.addArrayChild("song", createSongNode(context, track, context.getUser()));
            }

            return response;
        }

        Release::pointer getReleaseFromDirectory(Session& session, DirectoryId directoryId)
        {
            auto transaction{ session.createReadTransaction() };

            Release::FindParameters params;
            params.setDirectory(directoryId);
            params.setRange(Range{ 0, 1 }); // only support 1 directory <-> 1 release

            Release::pointer res;
            Release::find(session, params, [&](const Release::pointer& release) {
                res = release;
            });

            return res;
        }
    } // namespace

    Response handleGetMusicFoldersRequest(RequestContext& context)
    {
        Response response{ Response::createOkResponse(context.getServerProtocolVersion()) };
        Response::Node& musicFoldersNode{ response.createNode("musicFolders") };

        auto transaction{ context.getDbSession().createReadTransaction() };
        MediaLibrary::find(context.getDbSession(), [&](const MediaLibrary::pointer& library) {
            Response::Node& musicFolderNode{ musicFoldersNode.createArrayChild("musicFolder") };

            musicFolderNode.setAttribute("id", library->getId().getValue());
            musicFolderNode.setAttribute("name", library->getName());
        });

        return response;
    }

    Response handleGetIndexesRequest(RequestContext& context)
    {
        // Optional params
        const MediaLibraryId mediaLibrary{ getParameterAs<MediaLibraryId>(context.getParameters(), "musicFolderId").value_or(MediaLibraryId{}) };

        Response response{ Response::createOkResponse(context.getServerProtocolVersion()) };
        Response::Node& indexesNode{ response.createNode("indexes") };
        indexesNode.setAttribute("ignoredArticles", "");
        indexesNode.setAttribute("lastModified", reportedDummyDateULong); // TODO report last file write?

        auto transaction{ context.getDbSession().createReadTransaction() };

        const std::vector<Directory::pointer> rootDirectories{ getRootDirectories(context.getDbSession(), mediaLibrary) };

        IndexMap indexedDirectories;
        for (const Directory::pointer& rootdirectory : rootDirectories)
        {
            Track::FindParameters params;
            params.setDirectory(rootdirectory->getId());

            Track::find(context.getDbSession(), params, [&](const Track::pointer& track) {
                indexesNode.addArrayChild("child", createSongNode(context, track, context.getUser()));
            });

            getIndexedChildDirectories(context, rootdirectory, indexedDirectories);
        }

        for (const auto& [index, directories] : indexedDirectories)
        {
            Response::Node& indexNode{ indexesNode.createArrayChild("index") };
            indexNode.setAttribute("name", std::string{ index });

            for (const Directory::pointer& directory : directories)
            {
                // Legacy behavior: all sub directories are considered as artists (even if this is just containing an album, or just an intermediary directory)

                Response::Node childNode;
                childNode.setAttribute("id", idToString(directory->getId()));
                childNode.setAttribute("name", directory->getName());

                indexNode.addArrayChild("artist", std::move(childNode));
            }
        }

        return response;
    }

    Response handleGetMusicDirectoryRequest(RequestContext& context)
    {
        // Mandatory params
        const auto directoryId{ getMandatoryParameterAs<DirectoryId>(context.getParameters(), "id") };

        Response response{ Response::createOkResponse(context.getServerProtocolVersion()) };
        Response::Node& directoryNode{ response.createNode("directory") };

        auto transaction{ context.getDbSession().createReadTransaction() };

        const Directory::pointer directory{ Directory::find(context.getDbSession(), directoryId) };
        if (!directory)
            throw RequestedDataNotFoundError{};

        if (const Release::pointer release{ getReleaseFromDirectory(context.getDbSession(), directoryId) })
        {
            directoryNode.setAttribute("playCount", core::Service<scrobbling::IScrobblingService>::get()->getCount(context.getUser()->getId(), release->getId()));
            if (const Wt::WDateTime dateTime{ core::Service<feedback::IFeedbackService>::get()->getStarredDateTime(context.getUser()->getId(), release->getId()) }; dateTime.isValid())
                directoryNode.setAttribute("starred", core::stringUtils::toISO8601String(dateTime));
        }

        directoryNode.setAttribute("id", idToString(directory->getId()));
        directoryNode.setAttribute("name", directory->getName());
        // Original Subsonic does not report parent if this the parent directory is the root directory
        if (const Directory::pointer parentDirectory{ directory->getParentDirectory() })
            directoryNode.setAttribute("parent", idToString(parentDirectory->getId()));

        // list all sub directories
        {
            Directory::FindParameters params;
            params.setParentDirectory(directory->getId());
            params.setSortMethod(DirectorySortMethod::Name);

            Directory::find(context.getDbSession(), params, [&](const Directory::pointer& subDirectory) {
                const Release::pointer release{ getReleaseFromDirectory(context.getDbSession(), subDirectory->getId()) };

                if (release)
                {
                    directoryNode.addArrayChild("child", createAlbumNode(context, release, false, subDirectory));
                }
                else
                {
                    Response::Node childNode;
                    childNode.setAttribute("id", idToString(subDirectory->getId()));
                    childNode.setAttribute("title", subDirectory->getName());
                    childNode.setAttribute("isDir", true);
                    childNode.setAttribute("parent", idToString(directory->getId()));

                    directoryNode.addArrayChild("child", std::move(childNode));
                }
            });
        }

        // list all tracks
        {
            Track::FindParameters params;
            params.setDirectory(directory->getId());
            params.setSortMethod(TrackSortMethod::AbsoluteFilePath);

            Track::find(context.getDbSession(), params, [&](const Track::pointer& track) {
                directoryNode.addArrayChild("child", createSongNode(context, track, context.getUser()));
            });
        }

        return response;
    }

    Response handleGetGenresRequest(RequestContext& context)
    {
        Response response{ Response::createOkResponse(context.getServerProtocolVersion()) };

        Response::Node& genresNode{ response.createNode("genres") };

        auto transaction{ context.getDbSession().createReadTransaction() };

        const ClusterType::pointer clusterType{ ClusterType::find(context.getDbSession(), "GENRE") };
        if (clusterType)
        {
            const auto clusters{ clusterType->getClusters() };

            for (const Cluster::pointer& cluster : clusters)
                genresNode.addArrayChild("genre", createGenreNode(context, cluster));
        }

        return response;
    }

    Response handleGetArtistsRequest(RequestContext& context)
    {
        // Optional params
        const MediaLibraryId mediaLibrary{ getParameterAs<MediaLibraryId>(context.getParameters(), "musicFolderId").value_or(MediaLibraryId{}) };

        Response response{ Response::createOkResponse(context.getServerProtocolVersion()) };

        Response::Node& artistsNode{ response.createNode("artists") };
        artistsNode.setAttribute("ignoredArticles", "");
        artistsNode.setAttribute("lastModified", reportedDummyDateULong); // TODO report last file write?

        Artist::FindParameters parameters;
        {
            auto transaction{ context.getDbSession().createReadTransaction() };

            parameters.setSortMethod(ArtistSortMethod::SortName);
            switch (context.getUser()->getSubsonicArtistListMode())
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
        parameters.filters.setMediaLibrary(mediaLibrary);

        // This endpoint does not scale: make sort lived transactions in order not to block the whole application

        // first pass: dispatch the artists by first letter
        LMS_LOG(API_SUBSONIC, DEBUG, "GetArtists: fetching all artists...");
        std::map<char, std::vector<ArtistId>> artistsSortedByFirstChar;
        std::size_t currentArtistOffset{ 0 };
        constexpr std::size_t batchSize{ 100 };
        bool hasMoreArtists{ true };
        while (hasMoreArtists)
        {
            auto transaction{ context.getDbSession().createReadTransaction() };

            parameters.setRange(Range{ currentArtistOffset, batchSize });
            const auto artists{ Artist::find(context.getDbSession(), parameters) };
            for (const Artist::pointer& artist : artists.results)
            {
                std::string_view sortName{ artist->getSortName() };

                char sortChar;
                if (sortName.empty() || !std::isalpha(sortName[0]))
                    sortChar = '#';
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
                auto transaction{ context.getDbSession().createReadTransaction() };

                if (const Artist::pointer artist{ Artist::find(context.getDbSession(), artistId) })
                    indexNode.addArrayChild("artist", createArtistNode(context, artist));
            }
        }

        return response;
    }

    Response handleGetArtistRequest(RequestContext& context)
    {
        // Mandatory params
        ArtistId id{ getMandatoryParameterAs<ArtistId>(context.getParameters(), "id") };

        auto transaction{ context.getDbSession().createReadTransaction() };

        const Artist::pointer artist{ Artist::find(context.getDbSession(), id) };
        if (!artist)
            throw RequestedDataNotFoundError{};

        Response response{ Response::createOkResponse(context.getServerProtocolVersion()) };
        Response::Node artistNode{ createArtistNode(context, artist) };

        const auto releases{ Release::find(context.getDbSession(), Release::FindParameters{}.setArtist(artist->getId())) };
        for (const Release::pointer& release : releases.results)
            artistNode.addArrayChild("album", createAlbumNode(context, release, true /* id3 */));

        response.addNode("artist", std::move(artistNode));

        return response;
    }

    Response handleGetAlbumRequest(RequestContext& context)
    {
        // Mandatory params
        ReleaseId id{ getMandatoryParameterAs<ReleaseId>(context.getParameters(), "id") };

        auto transaction{ context.getDbSession().createReadTransaction() };

        Release::pointer release{ Release::find(context.getDbSession(), id) };
        if (!release)
            throw RequestedDataNotFoundError{};

        Response response{ Response::createOkResponse(context.getServerProtocolVersion()) };
        Response::Node albumNode{ createAlbumNode(context, release, true /* id3 */) };

        const auto tracks{ Track::find(context.getDbSession(), Track::FindParameters{}.setRelease(id).setSortMethod(TrackSortMethod::Release)) };
        for (const Track::pointer& track : tracks.results)
            albumNode.addArrayChild("song", createSongNode(context, track, true /* id3 */));

        response.addNode("album", std::move(albumNode));

        return response;
    }

    Response handleGetSongRequest(RequestContext& context)
    {
        // Mandatory params
        TrackId id{ getMandatoryParameterAs<TrackId>(context.getParameters(), "id") };

        auto transaction{ context.getDbSession().createReadTransaction() };

        const Track::pointer track{ Track::find(context.getDbSession(), id) };
        if (!track)
            throw RequestedDataNotFoundError{};

        Response response{ Response::createOkResponse(context.getServerProtocolVersion()) };
        response.addNode("song", createSongNode(context, track, context.getUser()));

        return response;
    }

    Response handleGetArtistInfo2Request(RequestContext& context)
    {
        // Mandatory params
        ArtistId id{ getMandatoryParameterAs<ArtistId>(context.getParameters(), "id") };

        // Optional params
        std::size_t count{ getParameterAs<std::size_t>(context.getParameters(), "count").value_or(20) };

        Response response{ Response::createOkResponse(context.getServerProtocolVersion()) };
        Response::Node& artistInfoNode{ response.createNode(Response::Node::Key{ "artistInfo2" }) };

        {
            auto transaction{ context.getDbSession().createReadTransaction() };

            const Artist::pointer artist{ Artist::find(context.getDbSession(), id) };
            if (!artist)
                throw RequestedDataNotFoundError{};

            if (const std::optional<core::UUID> artistMBID{ artist->getMBID() })
            {
                switch (context.getResponseFormat())
                {
                case ResponseFormat::json:
                    artistInfoNode.setAttribute("musicBrainzId", artistMBID->getAsString());
                    break;
                case ResponseFormat::xml:
                    artistInfoNode.createChild("musicBrainzId").setValue(artistMBID->getAsString());
                    break;
                }
            }

            ArtistInfo::find(context.getDbSession(), id, Range{ .offset = 0, .size = 1 }, [&](const ArtistInfo::pointer& artistInfo) {
                if (!artistInfo->getBiography().empty())
                {
                    switch (context.getResponseFormat())
                    {
                    case ResponseFormat::json:
                        artistInfoNode.setAttribute("biography", artistInfo->getBiography());
                        break;
                    case ResponseFormat::xml:
                        artistInfoNode.createChild("biography").setValue(artistInfo->getBiography());
                        break;
                    }
                }
            });
        }

        auto similarArtistsId{ core::Service<recommendation::IRecommendationService>::get()->getSimilarArtists(id, { TrackArtistLinkType::Artist, TrackArtistLinkType::ReleaseArtist }, count) };

        {
            auto transaction{ context.getDbSession().createReadTransaction() };

            for (const ArtistId similarArtistId : similarArtistsId)
            {
                const Artist::pointer similarArtist{ Artist::find(context.getDbSession(), similarArtistId) };
                if (similarArtist)
                    artistInfoNode.addArrayChild("similarArtist", createArtistNode(context, similarArtist));
            }
        }

        return response;
    }

    Response handleGetAlbumInfo(RequestContext& context)
    {
        const db::DirectoryId directoryId{ getMandatoryParameterAs<db::DirectoryId>(context.getParameters(), "id") };

        Response response{ Response::createOkResponse(context.getServerProtocolVersion()) };

        {
            auto transaction{ context.getDbSession().createReadTransaction() };

            if (db::Release::pointer release{ getReleaseFromDirectory(context.getDbSession(), directoryId) })
                response.addNode("albumInfo", createAlbumInfoNode(context, release));
        }
        return response;
    }

    Response handleGetAlbumInfo2(RequestContext& context)
    {
        const db::ReleaseId releaseId{ getMandatoryParameterAs<db::ReleaseId>(context.getParameters(), "id") };

        Response response{ Response::createOkResponse(context.getServerProtocolVersion()) };

        {
            auto transaction{ context.getDbSession().createReadTransaction() };

            if (db::Release::pointer release{ db::Release::find(context.getDbSession(), releaseId) })
                response.addNode("albumInfo", createAlbumInfoNode(context, release));
        }

        return response;
    }

    Response handleGetSimilarSongsRequest(RequestContext& context)
    {
        return handleGetSimilarSongsRequestCommon(context, false /* no id3 */);
    }

    Response handleGetSimilarSongs2Request(RequestContext& context)
    {
        return handleGetSimilarSongsRequestCommon(context, true /* id3 */);
    }

    Response handleGetTopSongs(RequestContext& context)
    {
        // Mandatory params
        std::string_view artistName{ getMandatoryParameterAs<std::string_view>(context.getParameters(), "artist") };
        std::size_t count{ getParameterAs<std::size_t>(context.getParameters(), "count").value_or(50) };
        if (count > defaultMaxCountSize)
            throw ParameterValueTooHighGenericError{ "count", defaultMaxCountSize };

        auto transaction{ context.getDbSession().createReadTransaction() };

        Response response{ Response::createOkResponse(context.getServerProtocolVersion()) };
        Response::Node& topSongs{ response.createNode("topSongs") };

        const auto artists{ Artist::find(context.getDbSession(), artistName) };
        if (artists.size() == 1)
        {
            scrobbling::IScrobblingService::FindParameters params;
            params.setUser(context.getUser()->getId());
            params.setRange(db::Range{ 0, count });
            params.setArtist(artists.front()->getId());

            const auto trackIds{ core::Service<scrobbling::IScrobblingService>::get()->getTopTracks(params) };
            for (const TrackId trackId : trackIds.results)
            {
                if (Track::pointer track{ Track::find(context.getDbSession(), trackId) })
                    topSongs.addArrayChild("song", createSongNode(context, track, context.getUser()));
            }
        }

        return response;
    }
} // namespace lms::api::subsonic
