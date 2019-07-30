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

#include <cstdlib>

#include <boost/filesystem.hpp>

#include "database/Artist.hpp"
#include "database/Cluster.hpp"
#include "database/Database.hpp"
#include "database/TrackList.hpp"
#include "database/Release.hpp"
#include "database/Track.hpp"
#include "database/User.hpp"

using namespace Database;

class ScopedFileDeleter final
{
	public:
		ScopedFileDeleter(const boost::filesystem::path& path) : _path {path} {}
		~ScopedFileDeleter() { boost::filesystem::remove(_path); }
	private:
		boost::filesystem::path _path;
};

#define CHECK(PRED)  \
{ \
	if (!(PRED)) \
	{ \
		std::string msg {"Predicate '" + std::string {#PRED} + "' at " + __FUNCTION__ + "@l." + std::to_string(__LINE__)}; \
		throw std::runtime_error(msg.c_str()); \
	} \
}

static
void
testRemoveDefaultEntries(Session& session)
{
	{
		auto transaction {session.createUniqueTransaction()};

		auto clusterTypes {ClusterType::getAll(session)};
		for (auto& clusterType : clusterTypes)
			clusterType.remove();
	}
}

static
void
testSingleTrack(Session& session)
{
	IdType trackId {};
	{
		auto transaction {session.createUniqueTransaction()};

		auto track {Track::create(session, "MyTrackFile")};
		CHECK(track);
		CHECK(IdIsValid(track.id()));
		trackId = track.id();
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto track {Track::getById(session, trackId)};
		CHECK(track);
		CHECK(Track::getAll(session).size() == 1);

		track.remove();
	}
}

static
void
testSingleArtist(Session& session)
{
	IdType artistId {};
	{
		auto transaction {session.createUniqueTransaction()};

		auto artist {Artist::create(session, "MyArtist")};
		CHECK(artist);
		artistId = artist.id();
	}

	{
		auto transaction {session.createSharedTransaction()};

		CHECK(Artist::getAll(session).size() == 1);

		auto artists {Artist::getAllOrphans(session)};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artistId);
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto artist {Artist::getById(session, artistId)};
		CHECK(artist);
		artist.remove();
	}
}

static
void
testSingleRelease(Session& session)
{
	IdType releaseId {};
	{
		auto transaction {session.createUniqueTransaction()};

		auto release {Release::create(session, "MyRelease")};
		CHECK(release);
		releaseId = release.id();
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto releases {Release::getAllOrphans(session)};
		CHECK(releases.size() == 1);
		CHECK(releases.front().id() == releaseId);

		releases = Release::getAll(session);
		CHECK(releases.size() == 1);
		CHECK(releases.front().id() == releaseId);
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto release {Release::getById(session, releaseId)};
		CHECK(release);
		release.remove();
	}
}

static
void
testSingleCluster(Session& session)
{
	IdType clusterTypeId {};
	IdType clusterId {};
	{
		auto transaction {session.createUniqueTransaction()};

		auto clusterType {ClusterType::create(session, "MyType")};
		CHECK(clusterType);
		auto cluster {Cluster::create(session, clusterType, "MyCluster")};
		CHECK(cluster);
		clusterTypeId = clusterType.id();
		clusterId = cluster.id();
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto clusters {Cluster::getAll(session)};
		CHECK(clusters.size() == 1);
		CHECK(clusters.front().id() == clusterId);
		CHECK(clusters.front()->getType().id() == clusterTypeId);

		clusters = Cluster::getAllOrphans(session);
		CHECK(clusters.size() == 1);
		CHECK(clusters.front().id() == clusterId);

		auto clusterTypes {ClusterType::getAll(session)};
		CHECK(clusterTypes.size() == 1);
		CHECK(clusterTypes.front().id() == clusterTypeId);

		clusterTypes = ClusterType::getAllOrphans(session);
		CHECK(clusterTypes.empty());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto cluster {Cluster::getById(session, clusterId)};
		CHECK(cluster);
		cluster.remove();

		auto clusterTypes {ClusterType::getAllOrphans(session)};
		CHECK(clusterTypes.size() == 1);
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto clusterType {ClusterType::getById(session, clusterTypeId)};
		CHECK(clusterType);

		clusterType.remove();
	}
}

static
void
testSingleTrackSingleArtist(Session& session)
{
	IdType trackId {};
	IdType artistId {};
	{
		auto transaction {session.createUniqueTransaction()};

		auto track {Track::create(session, "dummy")};
		CHECK(track);
		auto artist {Artist::create(session, "dummy")};
		CHECK(artist);

		auto trackArtistLink {TrackArtistLink::create(session, track, artist, TrackArtistLink::Type::Artist)};

		trackId = track.id();
		artistId = artist.id();
	}

	{
		auto transaction {session.createSharedTransaction()};
		CHECK(Artist::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto track {Track::getById(session, trackId)};
		CHECK(track);

		auto artists {track->getArtists()};
		CHECK(artists.size() == 1);
		auto artist {artists.front()};
		CHECK(artist.id() == artistId);

		CHECK(artist->getReleaseCount() == 0);

		CHECK(track->getArtistLinks().size() == 1);
		auto artistLink {track->getArtistLinks().front()};
		CHECK(artistLink->getTrack().id() == trackId);
		CHECK(artistLink->getArtist().id() == artistId);

		CHECK(track->getArtists(TrackArtistLink::Type::Artist).size() == 1);
		CHECK(track->getArtists(TrackArtistLink::Type::ReleaseArtist).empty());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto artist {Artist::getById(session, artistId)};
		CHECK(artist);
		auto tracks {artist->getTracks()};
		CHECK(tracks.size() == 1);
		auto track {tracks.front()};
		CHECK(track.id() == trackId);

		CHECK(artist->getTracks(TrackArtistLink::Type::ReleaseArtist).empty());
		CHECK(artist->getTracks(TrackArtistLink::Type::Artist).size() == 1);

		track.remove();
		artist.remove();
	}
}

static
void
testSingleTrackSingleArtistMultiRoles(Session& session)
{
	IdType trackId {};
	IdType artistId {};
	{
		auto transaction {session.createUniqueTransaction()};

		auto track {Track::create(session, "MyTrack")};
		auto artist {Artist::create(session, "MyArtist")};

		TrackArtistLink::create(session, track, artist, TrackArtistLink::Type::Artist);
		TrackArtistLink::create(session, track, artist, TrackArtistLink::Type::ReleaseArtist);
		TrackArtistLink::create(session, track, artist, TrackArtistLink::Type::Writer);

		trackId = track.id();
		artistId = artist.id();
	}

	{
		auto transaction {session.createSharedTransaction()};
		CHECK(Artist::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto track {Track::getById(session, trackId)};
		CHECK(track);

		auto artists {track->getArtists(TrackArtistLink::Type::Artist)};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artistId);

		artists = track->getArtists(TrackArtistLink::Type::ReleaseArtist);
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artistId);

		CHECK(track->getArtistLinks().size() == 3);

		auto artist {Artist::getById(session, artistId)};
		CHECK(artist);
		CHECK(artist->getTracks().size() == 1);
		CHECK(artist->getTracks(TrackArtistLink::Type::ReleaseArtist).size() == 1);
		CHECK(artist->getTracks(TrackArtistLink::Type::Artist).size() == 1);
		CHECK(artist->getTracks(TrackArtistLink::Type::Writer).size() == 1);
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto artist {Artist::getById(session, artistId)};
		auto track {Track::getById(session, trackId)};

		track.remove();
		artist.remove();
	}
}

static
void
testSingleTrackMultiArtists(Session& session)
{
	IdType trackId {};
	IdType artist1Id {};
	IdType artist2Id {};
	{
		auto transaction {session.createUniqueTransaction()};

		auto track {Track::create(session, "dummy")};
		auto artist1 {Artist::create(session, "artist1")};
		auto artist2 {Artist::create(session, "artist2")};

		TrackArtistLink::create(session, track, artist1, TrackArtistLink::Type::Artist);
		TrackArtistLink::create(session, track, artist2, TrackArtistLink::Type::Artist);

		trackId = track.id();
		artist1Id = artist1.id();
		artist2Id = artist2.id();

		CHECK(artist1Id != artist2Id);
	}

	{
		auto transaction {session.createSharedTransaction()};
		CHECK(Artist::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto track {Track::getById(session, trackId)};
		CHECK(track);

		auto artists {track->getArtists()};
		CHECK(artists.size() == 2);
		CHECK((artists[0].id() == artist1Id && artists[1].id() == artist2Id)
			|| (artists[0].id() == artist2Id && artists[1].id() == artist1Id));

		CHECK(track->getArtists(TrackArtistLink::Type::Artist).size() == 2);
		CHECK(track->getArtists(TrackArtistLink::Type::ReleaseArtist).empty());
		CHECK(Artist::getAll(session).size() == 2);
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto artist1 {Artist::getById(session, artist1Id)};
		CHECK(artist1);
		auto tracks {artist1->getTracks()};
		CHECK(tracks.size() == 1);
		auto track {tracks.front()};
		CHECK(track.id() == trackId);
		auto artist2 {Artist::getById(session, artist2Id)};
		CHECK(artist2);
		CHECK(artist2->getTracks().front() == track);

		CHECK(artist1->getTracks(TrackArtistLink::Type::ReleaseArtist).empty());
		CHECK(artist1->getTracks(TrackArtistLink::Type::Artist).size() == 1);
		CHECK(artist2->getTracks(TrackArtistLink::Type::ReleaseArtist).empty());
		CHECK(artist2->getTracks(TrackArtistLink::Type::Artist).size() == 1);

		track.remove();
		artist1.remove();
		artist2.remove();
	}
}

static
void
testSingleTrackSingleRelease(Session& session)
{
	IdType trackId {};
	IdType releaseId {};
	{
		auto transaction {session.createUniqueTransaction()};

		auto track {Track::create(session, "dummy")};
		CHECK(track);
		auto release {Release::create(session, "dummy")};
		CHECK(release);

		track.modify()->setRelease(release);

		trackId = track.id();
		releaseId = release.id();
	}

	{
		auto transaction {session.createSharedTransaction()};
		CHECK(Release::getAllOrphans(session).empty());

		auto release {Release::getById(session, releaseId)};
		CHECK(release);
		CHECK(release->getTracks().size() == 1);
		CHECK(release->getTracks().front().id() == trackId);
		CHECK(release->getTracksCount() == 1);
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto track {Track::getById(session, trackId)};
		CHECK(track);
		CHECK(track->getRelease());
		CHECK(track->getRelease().id() == releaseId);
		track.remove();
	}

	{
		auto transaction {session.createUniqueTransaction()};

		CHECK(Release::getAllOrphans(session).size() == 1);
		auto release {Release::getById(session, releaseId)};
		CHECK(release);
		CHECK(release->getTracks().empty());

		release.remove();
	}
}

static
void
testSingleTrackSingleCluster(Session& session)
{
	IdType trackId {};
	IdType clusterId {};
	IdType clusterTypeId {};
	{
		auto transaction {session.createUniqueTransaction()};

		auto track {Track::create(session, "dummy")};
		auto clusterType {ClusterType::create(session, "MyType")};
		auto cluster {Cluster::create(session, clusterType, "MyCluster")};

		cluster.modify()->addTrack(track);

		trackId = track.id();
		clusterTypeId = clusterType.id();
		clusterId = cluster.id();
	}

	{
		auto transaction {session.createSharedTransaction()};
		CHECK(Cluster::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto tracks {Track::getByFilter(session, {clusterId})};
		CHECK(tracks.size() == 1);
		CHECK(tracks.front().id() == trackId);

		tracks = Track::getByFilter(session, {});
		CHECK(tracks.size() == 1);
		CHECK(tracks.front().id() == trackId);
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto track {Track::getById(session, trackId)};
		auto cluster {Cluster::getById(session, clusterId)};
		auto clusterType {ClusterType::getById(session, clusterTypeId)};

		track.remove();
		cluster.remove();
		clusterType.remove();
	}
}

static
void
testSingleTrackSingleReleaseSingleCluster(Session& session)
{
	IdType trackId {};
	IdType releaseId {};
	IdType clusterId {};
	IdType clusterTypeId {};
	{
		auto transaction {session.createUniqueTransaction()};

		auto track {Track::create(session, "MyTrackFile")};
		auto release {Release::create(session, "MyRelease")};
		auto clusterType {ClusterType::create(session, "MyType")};
		auto cluster {Cluster::create(session, clusterType, "MyCluster")};

		track.modify()->setRelease(release);
		cluster.modify()->addTrack(track);

		trackId = track.id();
		releaseId = release.id();
		clusterTypeId = clusterType.id();
		clusterId = cluster.id();
	}

	{
		auto transaction {session.createSharedTransaction()};
		CHECK(Cluster::getAllOrphans(session).empty());
		CHECK(Release::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto releases {Release::getByFilter(session, {clusterId})};
		CHECK(releases.size() == 1);
		CHECK(releases.front().id() == releaseId);

		releases = Release::getByFilter(session, {clusterId});
		CHECK(releases.size() == 1);
		CHECK(releases.front().id() == releaseId);
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto cluster {Cluster::getById(session, clusterId)};
		CHECK(cluster->getReleasesCount() == 1);
		CHECK(cluster->getTracksCount() == 1);
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto track {Track::getById(session, trackId)};
		auto release {Release::getById(session, releaseId)};
		auto cluster {Cluster::getById(session, clusterId)};
		auto clusterType {ClusterType::getById(session, clusterTypeId)};

		track.remove();
		release.remove();
		cluster.remove();
		clusterType.remove();
	}
}

static
void
testSingleTrackSingleArtistMultiClusters(Session& session)
{
	IdType trackId {};
	IdType artistId {};
	IdType cluster1Id {};
	IdType cluster2Id {};
	IdType clusterTypeId {};
	{
		auto transaction {session.createUniqueTransaction()};

		auto track {Track::create(session, "MyTrackFile")};
		auto artist {Artist::create(session, "MyArtist")};
		auto clusterType {ClusterType::create(session, "MyType")};
		auto cluster1 {Cluster::create(session, clusterType, "MyCluster1")};
		auto cluster2 {Cluster::create(session, clusterType, "MyCluster2")};

		auto trackArtistLink {TrackArtistLink::create(session, track, artist, TrackArtistLink::Type::Artist)};
		cluster1.modify()->addTrack(track);

		trackId = track.id();
		artistId = artist.id();
		clusterTypeId = clusterType.id();
		cluster1Id = cluster1.id();
		cluster2Id = cluster2.id();
	}

	{
		auto transaction {session.createSharedTransaction()};
		CHECK(Cluster::getAllOrphans(session).empty());
		CHECK(Release::getAllOrphans(session).empty());
		CHECK(Artist::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {Artist::getByFilter(session, {cluster1Id})};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artistId);

		artists = Artist::getByFilter(session, {});
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artistId);

		artists = Artist::getByFilter(session, {cluster2Id});
		CHECK(artists.empty());

		auto cluster2 {Cluster::getById(session, cluster2Id)};
		auto track {Track::getById(session, trackId)};
		cluster2.modify()->addTrack(track);
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {Artist::getByFilter(session, {cluster1Id})};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artistId);

		artists = Artist::getByFilter(session, {cluster2Id});
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artistId);

		artists = Artist::getByFilter(session, {cluster1Id, cluster2Id});
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artistId);
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto track {Track::getById(session, trackId)};
		auto artist {Artist::getById(session, artistId)};
		auto cluster1 {Cluster::getById(session, cluster1Id)};
		auto cluster2 {Cluster::getById(session, cluster2Id)};
		auto clusterType {ClusterType::getById(session, clusterTypeId)};

		track.remove();
		artist.remove();
		cluster1.remove();
		cluster2.remove();
		clusterType.remove();
	}
}

static
void
testSingleTrackSingleArtistMultiRolesMultiClusters(Session& session)
{
	IdType trackId {};
	IdType artistId {};
	IdType clusterId {};
	IdType clusterTypeId {};
	{
		auto transaction {session.createUniqueTransaction()};

		auto track {Track::create(session, "MyTrackFile")};
		auto artist {Artist::create(session, "MyArtist")};
		auto clusterType {ClusterType::create(session, "MyType")};
		auto cluster {Cluster::create(session, clusterType, "MyCluster")};

		TrackArtistLink::create(session, track, artist, TrackArtistLink::Type::Artist);
		TrackArtistLink::create(session, track, artist, TrackArtistLink::Type::ReleaseArtist);
		cluster.modify()->addTrack(track);

		trackId = track.id();
		artistId = artist.id();
		clusterTypeId = clusterType.id();
		clusterId = cluster.id();
	}

	{
		auto transaction {session.createSharedTransaction()};
		CHECK(Cluster::getAllOrphans(session).empty());
		CHECK(Release::getAllOrphans(session).empty());
		CHECK(Artist::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {Artist::getByFilter(session, {clusterId})};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artistId);
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto track {Track::getById(session, trackId)};
		auto artist {Artist::getById(session, artistId)};
		auto cluster {Cluster::getById(session, clusterId)};
		auto clusterType {ClusterType::getById(session, clusterTypeId)};

		track.remove();
		artist.remove();
		cluster.remove();
		clusterType.remove();
	}
}

static
void
testMultiTracksSingleArtistMultiClusters(Session& session)
{
	const std::size_t nbTracks {10};
	const std::size_t nbClusters {5};
	IdType artistId {};
	IdType clusterTypeId {};
	{
		auto transaction {session.createUniqueTransaction()};

		auto artist {Artist::create(session, "MyArtist")};

		auto clusterType {ClusterType::create(session, "MyType")};
		std::vector<Cluster::pointer> clusters;

		for (std::size_t i {}; i < nbClusters; ++i)
			clusters.push_back(Cluster::create(session, clusterType, "MyCluster" + std::to_string(i)));

		for (std::size_t i {}; i < nbTracks; ++i)
		{
			auto track {Track::create(session, "MyTrackFile")};
			TrackArtistLink::create(session, track, artist, TrackArtistLink::Type::Artist);

			for (const auto& cluster : clusters)
				cluster.modify()->addTrack(track);
		}

		artistId = artist.id();
		clusterTypeId = clusterType.id();
	}

	{
		auto transaction {session.createSharedTransaction()};
		CHECK(Cluster::getAllOrphans(session).empty());
		CHECK(Artist::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		std::vector<Cluster::pointer> clusters {Cluster::getAll(session)};
		CHECK(clusters.size() == nbClusters);

		std::set<IdType> clusterIds;
		std::transform(std::cbegin(clusters), std::cend(clusters), std::inserter(clusterIds, std::begin(clusterIds)), [](const Cluster::pointer& cluster) { return cluster.id(); });

		auto artists {Artist::getByFilter(session, clusterIds)};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artistId);
	}

	{
		auto transaction {session.createUniqueTransaction()};

		std::vector<Cluster::pointer> clusters {Cluster::getAll(session)};
		for (auto& cluster : clusters)
			cluster.remove();

		std::vector<Track::pointer> tracks {Track::getAll(session)};
		for (auto& track : tracks)
			track.remove();

		auto artist {Artist::getById(session, artistId)};
		auto clusterType {ClusterType::getById(session, clusterTypeId)};

		artist.remove();
		clusterType.remove();
	}
}

static
void
testMultiTracksSingleArtistSingleRelease(Session& session)
{
	const std::size_t nbTracks {10};
	IdType artistId {};
	IdType releaseId {};
	{
		auto transaction {session.createUniqueTransaction()};

		auto artist {Artist::create(session, "MyArtist")};
		auto release {Release::create(session, "MyRelease")};

		for (std::size_t i {}; i < nbTracks; ++i)
		{
			auto track {Track::create(session, "MyTrackFile")};
			TrackArtistLink::create(session, track, artist, TrackArtistLink::Type::Artist);
			track.modify()->setRelease(release);
		}

		artistId = artist.id();
		releaseId = release.id();
	}

	{
		auto transaction {session.createSharedTransaction()};
		CHECK(Release::getAllOrphans(session).empty());
		CHECK(Artist::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artist {Artist::getById(session, artistId)};
		CHECK(artist);
		CHECK(artist->getReleaseCount() == 1);
		CHECK(artist->getReleases().size() == 1);
		CHECK(artist->getReleases().front().id() == releaseId);

		auto release {Release::getById(session, releaseId)};
		CHECK(release);
		CHECK(release->getTracks().size() == nbTracks);
	}

	{
		auto transaction {session.createUniqueTransaction()};

		std::vector<Track::pointer> tracks {Track::getAll(session)};
		for (auto& track : tracks)
			track.remove();

		auto artist {Artist::getById(session, artistId)};
		auto release {Release::getById(session, releaseId)};
		artist.remove();
		release.remove();
	}
}

static
void
testSingleTrackSingleReleaseSingleArtist(Session& session)
{
	IdType trackId {};
	IdType releaseId {};
	IdType artistId {};
	{
		auto transaction {session.createUniqueTransaction()};

		auto track {Track::create(session, "dummy")};
		auto release {Release::create(session, "dummy")};
		auto artist {Artist::create(session, "dummy")};

		auto trackArtistLink {TrackArtistLink::create(session, track, artist, TrackArtistLink::Type::Artist)};
		track.modify()->setRelease(release);

		trackId = track.id();
		releaseId = release.id();
		artistId = artist.id();
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto artist {Artist::getById(session, artistId)};
		CHECK(artist);
		auto releases {artist->getReleases()};
		CHECK(releases.size() == 1);
		CHECK(releases.front().id() == releaseId);

		CHECK(artist->getReleaseCount() == 1);

		auto release {Release::getById(session, releaseId)};
		CHECK(release);
		auto artists {release->getArtists()};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artistId);

		auto track {Track::getById(session, trackId)};

		track.remove();
		artist.remove();
		release.remove();
	}
}

static
void
testSingleTrackSingleReleaseSingleArtistSingleCluster(Session& session)
{
	IdType trackId {};
	IdType releaseId {};
	IdType artistId {};
	IdType clusterId {};
	IdType clusterTypeId {};

	{
		auto transaction {session.createUniqueTransaction()};

		auto track {Track::create(session, "MyTrackFile")};
		auto release {Release::create(session, "MyRelease")};
		auto artist {Artist::create(session, "MyArtist")};

		auto clusterType {ClusterType::create(session, "MyType")};
		auto cluster {Cluster::create(session, clusterType, "MyCluster")};

		auto trackArtistLink {TrackArtistLink::create(session, track, artist, TrackArtistLink::Type::Artist)};
		track.modify()->setRelease(release);
		cluster.modify()->addTrack(track);

		trackId = track.id();
		releaseId = release.id();
		artistId = artist.id();
		clusterId = cluster.id();
		clusterTypeId = clusterType.id();
	}

	{
		auto transaction {session.createSharedTransaction()};

		CHECK(Cluster::getAllOrphans(session).empty());
		CHECK(Artist::getAllOrphans(session).empty());
		CHECK(Release::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {Artist::getByFilter(session, {clusterId})};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artistId);

		auto artist {Artist::getById(session, artistId)};
		auto releases {artist->getReleases()};
		CHECK(releases.size() == 1);
		CHECK(releases.front().id() == releaseId);

		releases = artist->getReleases({clusterId});
		CHECK(releases.size() == 1);
		CHECK(releases.front().id() == releaseId);
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto artist {Artist::getById(session, artistId)};
		auto release {Release::getById(session, releaseId)};
		auto track {Track::getById(session, trackId)};
		auto cluster {Cluster::getById(session, clusterId)};
		auto clusterType {ClusterType::getById(session, clusterTypeId)};

		track.remove();
		artist.remove();
		release.remove();
		cluster.remove();
		clusterType.remove();
	}
}

static
void
testSingleTrackSingleReleaseSingleArtistMultiClusters(Session& session)
{
	IdType trackId {};
	IdType releaseId {};
	IdType artistId {};
	IdType cluster1Id {};
	IdType cluster2Id {};
	IdType clusterTypeId {};

	{
		auto transaction {session.createUniqueTransaction()};

		auto track {Track::create(session, "MyTrackFile")};
		auto release {Release::create(session, "MyRelease")};
		auto artist {Artist::create(session, "MyArtist")};

		auto clusterType {ClusterType::create(session, "MyType")};
		auto cluster1 {Cluster::create(session, clusterType, "MyCluster1")};
		auto cluster2 {Cluster::create(session, clusterType, "MyCluster2")};

		auto trackArtistLink {TrackArtistLink::create(session, track, artist, TrackArtistLink::Type::Artist)};
		track.modify()->setRelease(release);
		cluster1.modify()->addTrack(track);
		cluster2.modify()->addTrack(track);

		trackId = track.id();
		releaseId = release.id();
		artistId = artist.id();
		cluster1Id = cluster1.id();
		cluster2Id = cluster2.id();
		clusterTypeId = clusterType.id();
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artist {Artist::getById(session, artistId)};
		auto releases {artist->getReleases()};
		CHECK(releases.size() == 1);
		CHECK(releases.front().id() == releaseId);

		releases = artist->getReleases({cluster1Id, cluster2Id});
		CHECK(releases.size() == 1);
		CHECK(releases.front().id() == releaseId);
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto artist {Artist::getById(session, artistId)};
		auto release {Release::getById(session, releaseId)};
		auto track {Track::getById(session, trackId)};
		auto cluster1 {Cluster::getById(session, cluster1Id)};
		auto cluster2 {Cluster::getById(session, cluster1Id)};
		auto clusterType {ClusterType::getById(session, clusterTypeId)};

		track.remove();
		artist.remove();
		release.remove();
		cluster1.remove();
		cluster2.remove();
		clusterType.remove();
	}
}

static
void
testSingleUser(Session& session)
{
	IdType userId {};
	{
		auto transaction {session.createUniqueTransaction()};

		auto user {User::create(session, "", {})};
		CHECK(user);

		userId = user.id();
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto user {User::getById(session, userId)};
		CHECK(user);
		CHECK(user->getPlayedTrackList(session)->getCount() == 0);
		CHECK(user->getPlayedTrackList(session)->getTopTracks(1).empty());
		CHECK(user->getPlayedTrackList(session)->getTopArtists(1).empty());
		CHECK(user->getPlayedTrackList(session)->getTopReleases(1).empty());
		CHECK(user->getQueuedTrackList(session)->getCount() == 0);
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto user {User::getById(session, userId)};
		CHECK(user);
		user.remove();
	}
}

static
void
testSingleStarredArtist(Session& session)
{
	IdType artistId {};
	IdType userId {};
	{
		auto transaction {session.createUniqueTransaction()};

		auto artist {Artist::create(session, "MyArtist")};
		CHECK(artist);
		auto user {User::create(session, "", {})};
		CHECK(user);

		artistId = artist.id();
		userId = user.id();
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto user {User::getById(session, userId)};
		CHECK(user);
		auto artist {Artist::getById(session, artistId)};
		CHECK(artist);

		user.modify()->starArtist(artist);
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto user {User::getById(session, userId)};
		CHECK(user);
		auto artists {user->getStarredArtists()};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artistId);

		auto artist {Artist::getById(session, artistId)};
		CHECK(user->hasStarredArtist(artist));
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto user {User::getById(session, userId)};
		CHECK(user);
		auto artist {Artist::getById(session, artistId)};
		CHECK(artist);

		user.remove();
		artist.remove();
	}
}

static
void
testSingleStarredRelease(Session& session)
{
	IdType releaseId {};
	IdType userId {};
	{
		auto transaction {session.createUniqueTransaction()};

		auto release {Release::create(session, "MyRelease")};
		CHECK(release);
		auto user {User::create(session, "", {})};
		CHECK(user);

		releaseId = release.id();
		userId = user.id();
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto user {User::getById(session, userId)};
		CHECK(user);
		auto release {Release::getById(session, releaseId)};
		CHECK(release);

		user.modify()->starRelease(release);
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto user {User::getById(session, userId)};
		CHECK(user);
		auto releases {user->getStarredReleases()};
		CHECK(releases.size() == 1);
		CHECK(releases.front().id() == releaseId);

		auto release {Release::getById(session, releaseId)};
		CHECK(user->hasStarredRelease(release));
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto user {User::getById(session, userId)};
		CHECK(user);
		auto release {Release::getById(session, releaseId)};
		CHECK(release);

		user.remove();
		release.remove();
	}
}

static
void
testSingleStarredTrack(Session& session)
{
	IdType trackId {};
	IdType userId {};
	{
		auto transaction {session.createUniqueTransaction()};

		auto track {Track::create(session, "MyTrackFile")};
		CHECK(track);
		auto user {User::create(session, "", {})};
		CHECK(user);

		trackId = track.id();
		userId = user.id();
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto user {User::getById(session, userId)};
		CHECK(user);
		auto track {Track::getById(session, trackId)};
		CHECK(track);

		user.modify()->starTrack(track);
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto user {User::getById(session, userId)};
		CHECK(user);
		auto tracks {user->getStarredTracks()};
		CHECK(tracks.size() == 1);
		CHECK(tracks.front().id() == trackId);

		auto track {Track::getById(session, trackId)};
		CHECK(user->hasStarredTrack(track));
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto user {User::getById(session, userId)};
		CHECK(user);
		auto track {Track::getById(session, trackId)};
		CHECK(track);

		user.remove();
		track.remove();
	}
}

static
void
testDatabaseEmpty(Session& session)
{
	auto uniqueTransaction {session.createUniqueTransaction()};

	CHECK(Artist::getAll(session).empty());
	CHECK(Cluster::getAll(session).empty());
	CHECK(ClusterType::getAll(session).empty());
	CHECK(Release::getAll(session).empty());
	CHECK(Track::getAll(session).empty());
}

int main(int argc, char* argv[])
{

	try
	{
		const boost::filesystem::path tmpFile {boost::filesystem::temp_directory_path() / boost::filesystem::unique_path()};
		ScopedFileDeleter tmpFileDeleter {tmpFile};

		std::cout << "Database test file: '" << tmpFile.string() << "'" << std::endl;

		for (std::size_t i = 0; i < 2; ++i)
		{
			Database::Database db {tmpFile};
			std::unique_ptr<Session> session {db.createSession()};

			auto runTest = [&session](const std::string& name, std::function<void(Session&)> testFunc)
			{
				std::cout << "Running test '" << name << "'..." << std::endl;
				testFunc(*session);
				testDatabaseEmpty(*session);
				std::cout << "Running test '" << name << "': SUCCESS" << std::endl;
			};

#define RUN_TEST(test)	runTest(#test, test)

			// Special test to remove any default created entries
			RUN_TEST(testRemoveDefaultEntries);

			RUN_TEST(testSingleTrack);
			RUN_TEST(testSingleArtist);
			RUN_TEST(testSingleRelease);
			RUN_TEST(testSingleCluster);

			RUN_TEST(testSingleTrackSingleArtist);
			RUN_TEST(testSingleTrackSingleArtistMultiRoles);
			RUN_TEST(testSingleTrackMultiArtists);

			RUN_TEST(testSingleTrackSingleRelease);

			RUN_TEST(testSingleTrackSingleCluster);

			RUN_TEST(testSingleTrackSingleReleaseSingleCluster);
			RUN_TEST(testSingleTrackSingleArtistMultiClusters);
			RUN_TEST(testSingleTrackSingleArtistMultiRolesMultiClusters);
			RUN_TEST(testMultiTracksSingleArtistMultiClusters);
			RUN_TEST(testMultiTracksSingleArtistSingleRelease);

			RUN_TEST(testSingleTrackSingleReleaseSingleArtist);

			RUN_TEST(testSingleTrackSingleReleaseSingleArtistSingleCluster);
			RUN_TEST(testSingleTrackSingleReleaseSingleArtistMultiClusters);

			RUN_TEST(testSingleUser);

			RUN_TEST(testSingleStarredArtist);
			RUN_TEST(testSingleStarredRelease);
			RUN_TEST(testSingleStarredTrack);

		}
	}
	catch (std::exception& e)
	{
		std::cerr << "Caught exception: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
