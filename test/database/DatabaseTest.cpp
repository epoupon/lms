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

#include <filesystem>
#include <list>

#include "database/Artist.hpp"
#include "database/Cluster.hpp"
#include "database/Db.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/TrackList.hpp"
#include "database/User.hpp"

#include "utils/StreamLogger.hpp"

using namespace Database;

#define CHECK(PRED)  \
	do \
	{ \
		try \
		{ \
			if (!(PRED)) \
			{ \
				std::string error {"Predicate FAILED '" + std::string {#PRED} + "' at " + __FUNCTION__ + "@l." + std::to_string(__LINE__)}; \
				std::cerr << error << std::endl; \
				throw std::runtime_error {error}; \
			} \
		} \
		catch (std::exception& e) \
		{ \
			std::cerr << "Exception caught: " << e.what() << std::endl; \
			throw; \
		} \
	} while (0)


class ScopedFileDeleter final
{
	public:
		ScopedFileDeleter(const std::filesystem::path& path) : _path {path} {}
		~ScopedFileDeleter() { std::filesystem::remove(_path); }

		ScopedFileDeleter(const ScopedFileDeleter&) = delete;
		ScopedFileDeleter(ScopedFileDeleter&&) = delete;
		ScopedFileDeleter operator=(const ScopedFileDeleter&) = delete;
		ScopedFileDeleter operator=(ScopedFileDeleter&&) = delete;

	private:
		const std::filesystem::path _path;
};

template <typename T>
class ScopedEntity
{
	public:

		template <typename... Args>
		ScopedEntity(Session& session, Args&& ...args)
			: _session {session}
		{
			auto transaction {_session.createUniqueTransaction()};

			auto entity {T::create(_session, std::forward<Args>(args)...)};
			CHECK(entity);
			_id = entity.id();
		}

		~ScopedEntity()
		{
			auto transaction {_session.createUniqueTransaction()};

			auto entity {T::getById(_session, _id)};
			entity.remove();
		}

		ScopedEntity(const ScopedEntity&) = delete;
		ScopedEntity(ScopedEntity&&) = delete;
		ScopedEntity& operator=(const ScopedEntity&) = delete;
		ScopedEntity& operator=(ScopedEntity&&) = delete;

		typename T::pointer lockAndGet()
		{
			auto transaction {_session.createSharedTransaction()};
			return get();
		}

		typename T::pointer get()
		{
			_session.checkSharedLocked();

			auto entity {T::getById(_session, _id)};
			CHECK(entity);
			return entity;
		}

		typename T::pointer operator->()
		{
			return get();
		}

		IdType getId() const { return _id; }

	private:
		Session& _session;
		IdType _id {};
};

using ScopedArtist = ScopedEntity<Artist>;
using ScopedCluster = ScopedEntity<Cluster>;
using ScopedClusterType = ScopedEntity<ClusterType>;
using ScopedRelease = ScopedEntity<Release>;
using ScopedTrack = ScopedEntity<Track>;
using ScopedTrackList = ScopedEntity<TrackList>;
using ScopedUser = ScopedEntity<User>;


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
	ScopedTrack track {session, "MyTrackFile"};

	{
		auto transaction {session.createUniqueTransaction()};

		CHECK(Track::getAll(session).size() == 1);
	}
}

static
void
testSingleArtist(Session& session)
{
	ScopedArtist artist {session, "MyArtist"};

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {Artist::getAll(session)};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist.getId());

		artists = Artist::getAllOrphans(session);
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist.getId());
	}
}

static
void
testSingleRelease(Session& session)
{
	ScopedRelease release {session, "MyRelease"};

	{
		auto transaction {session.createSharedTransaction()};

		auto releases {Release::getAllOrphans(session)};
		CHECK(releases.size() == 1);
		CHECK(releases.front().id() == release.getId());

		releases = Release::getAll(session);
		CHECK(releases.size() == 1);
		CHECK(releases.front().id() == release.getId());
	}
}

static
void
testSingleCluster(Session& session)
{
	ScopedClusterType clusterType {session, "MyType"};

	{
		ScopedCluster cluster {session, clusterType.lockAndGet(), "MyCluster"};

		{
			auto transaction {session.createUniqueTransaction()};


			auto clusters {Cluster::getAll(session)};
			CHECK(clusters.size() == 1);
			CHECK(clusters.front().id() == cluster.getId());
			CHECK(clusters.front()->getType().id() == clusterType.getId());

			clusters = Cluster::getAllOrphans(session);
			CHECK(clusters.size() == 1);
			CHECK(clusters.front().id() == cluster.getId());

			auto clusterTypes {ClusterType::getAll(session)};
			CHECK(clusterTypes.size() == 1);
			CHECK(clusterTypes.front().id() == clusterType.getId());

			clusterTypes = ClusterType::getAllOrphans(session);
			CHECK(clusterTypes.empty());
		}
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto clusterTypes {ClusterType::getAllOrphans(session)};
		CHECK(clusterTypes.size() == 1);
		CHECK(clusterTypes.front().id() == clusterType.getId());
	}
}

static
void
testSingleTrackSingleArtist(Session& session)
{
	ScopedTrack track {session, "MyTrack"};
	ScopedArtist artist {session, "MyArtist"};

	{
		auto transaction {session.createUniqueTransaction()};

		TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLink::Type::Artist);
	}

	{
		auto transaction {session.createSharedTransaction()};
		CHECK(Artist::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {track->getArtists()};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist.getId());

		CHECK(artist->getReleaseCount() == 0);

		CHECK(track->getArtistLinks().size() == 1);
		auto artistLink {track->getArtistLinks().front()};
		CHECK(artistLink->getTrack().id() == track.getId());
		CHECK(artistLink->getArtist().id() == artist.getId());

		CHECK(track->getArtists(TrackArtistLink::Type::Artist).size() == 1);
		CHECK(track->getArtists(TrackArtistLink::Type::ReleaseArtist).empty());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto tracks {artist->getTracks()};
		CHECK(tracks.size() == 1);
		CHECK(tracks.front().id() == track.getId());

		CHECK(artist->getTracks(TrackArtistLink::Type::ReleaseArtist).empty());
		CHECK(artist->getTracks(TrackArtistLink::Type::Artist).size() == 1);
	}
}

static
void
testSingleTrackSingleArtistMultiRoles(Session& session)
{
	ScopedTrack track {session, "MyTrack"};
	ScopedArtist artist {session, "MyArtist"};
	{
		auto transaction {session.createUniqueTransaction()};

		TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLink::Type::Artist);
		TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLink::Type::ReleaseArtist);
		TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLink::Type::Writer);
	}

	{
		auto transaction {session.createSharedTransaction()};
		CHECK(Artist::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {track->getArtists(TrackArtistLink::Type::Artist)};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist.getId());

		artists = track->getArtists(TrackArtistLink::Type::ReleaseArtist);
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist.getId());

		CHECK(track->getArtistLinks().size() == 3);

		CHECK(artist->getTracks().size() == 1);
		CHECK(artist->getTracks(TrackArtistLink::Type::ReleaseArtist).size() == 1);
		CHECK(artist->getTracks(TrackArtistLink::Type::Artist).size() == 1);
		CHECK(artist->getTracks(TrackArtistLink::Type::Writer).size() == 1);
	}
}

static
void
testSingleTrackMultiArtists(Session& session)
{
	ScopedTrack track {session, "track"};
	ScopedArtist artist1 {session, "artist1"};
	ScopedArtist artist2 {session, "artist2"};
	CHECK(artist1.getId() != artist2.getId());

	{
		auto transaction {session.createUniqueTransaction()};

		TrackArtistLink::create(session, track.get(), artist1.get(), TrackArtistLink::Type::Artist);
		TrackArtistLink::create(session, track.get(), artist2.get(), TrackArtistLink::Type::Artist);
	}

	{
		auto transaction {session.createSharedTransaction()};
		CHECK(Artist::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {track->getArtists()};
		CHECK(artists.size() == 2);
		CHECK((artists[0].id() == artist1.getId() && artists[1].id() == artist2.getId())
			|| (artists[0].id() == artist2.getId() && artists[1].id() == artist1.getId()));

		CHECK(track->getArtists(TrackArtistLink::Type::Artist).size() == 2);
		CHECK(track->getArtists(TrackArtistLink::Type::ReleaseArtist).empty());
		CHECK(Artist::getAll(session).size() == 2);
	}

	{
		auto transaction {session.createUniqueTransaction()};

		CHECK(artist1->getTracks().front() == track.get());
		CHECK(artist2->getTracks().front() == track.get());

		CHECK(artist1->getTracks(TrackArtistLink::Type::ReleaseArtist).empty());
		CHECK(artist1->getTracks(TrackArtistLink::Type::Artist).size() == 1);
		CHECK(artist2->getTracks(TrackArtistLink::Type::ReleaseArtist).empty());
		CHECK(artist2->getTracks(TrackArtistLink::Type::Artist).size() == 1);
	}
}

static
void
testSingleTrackSingleRelease(Session& session)
{
	ScopedRelease release {session, "MyRelease"};

	{
		ScopedTrack track {session, "MyTrack"};
		{
			auto transaction {session.createUniqueTransaction()};

			track.get().modify()->setRelease(release.get());
		}

		{
			auto transaction {session.createSharedTransaction()};
			CHECK(Release::getAllOrphans(session).empty());

			CHECK(release->getTracks().size() == 1);
			CHECK(release->getTracksCount() == 1);
			CHECK(release->getTracks().front().id() == track.getId());
		}

		{
			auto transaction {session.createUniqueTransaction()};

			CHECK(track->getRelease());
			CHECK(track->getRelease().id() == release.getId());
		}
	}

	{
		auto transaction {session.createUniqueTransaction()};

		CHECK(release->getTracks().empty());

		auto releases {Release::getAllOrphans(session)};
		CHECK(releases.size() == 1);
		CHECK(releases.front().id() == release.getId());
	}
}

static
void
testSingleTrackSingleCluster(Session& session)
{
	ScopedTrack track {session, "MyTrack"};
	ScopedClusterType clusterType {session, "MyClusterType"};

	{
		auto transaction {session.createSharedTransaction()};
		CHECK(Cluster::getAllOrphans(session).empty());
		auto clusterTypes {ClusterType::getAllOrphans(session)};
		CHECK(clusterTypes.size() ==1);
		CHECK(clusterTypes.front().id() == clusterType.getId());
	}

	ScopedCluster cluster1 {session, clusterType.lockAndGet(), "MyCluster1"};
	ScopedCluster cluster2 {session, clusterType.lockAndGet(), "MyCluster2"};

	{
		auto transaction {session.createSharedTransaction()};
		auto clusters {Cluster::getAllOrphans(session)};
		CHECK(clusters.size() == 2);
		CHECK(track->getClusters().empty());
		CHECK(track->getClusterIds().empty());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		cluster1.get().modify()->addTrack(track.get());
	}

	{
		auto transaction {session.createSharedTransaction()};
		auto clusters {Cluster::getAllOrphans(session)};
		CHECK(clusters.size() == 1);
		CHECK(clusters.front().id() == cluster2.getId());

		CHECK(ClusterType::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto tracks {Track::getByClusters(session, {cluster1.getId()})};
		CHECK(tracks.size() == 1);
		CHECK(tracks.front().id() == track.getId());

		tracks = Track::getByClusters(session, {cluster2.getId()});
		CHECK(tracks.empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto clusters {track->getClusters()};
		CHECK(clusters.size() == 1);
		CHECK(clusters.front().id() == cluster1.getId());

		auto clusterIds {track->getClusterIds()};
		CHECK(clusterIds.size() == 1);
		CHECK(clusterIds.front() == cluster1.getId());
	}
}

static
void
testMultipleTracksSingleCluster(Session& session)
{
	std::list<ScopedTrack> tracks;
	ScopedClusterType clusterType {session, "MyClusterType"};
	ScopedCluster cluster {session, clusterType.lockAndGet(), "MyClusterType"};

	for (std::size_t i {}; i < 10; ++i)
	{
		tracks.emplace_back(session, "MyTrack" + std::to_string(i));

		{
			auto transaction {session.createUniqueTransaction()};
			cluster.get().modify()->addTrack(tracks.back().get());
		}
	}

	{
		auto transaction {session.createSharedTransaction()};
		CHECK(Cluster::getAllOrphans(session).empty());

		CHECK(cluster->getTracksCount() == tracks.size());

		for (auto trackCluster : cluster->getTracks())
		{
			auto it {std::find_if(std::cbegin(tracks), std::cend(tracks), [&](const ScopedTrack& track) { return trackCluster.id() == track.getId(); })};
			CHECK(it != std::cend(tracks));
		}
	}
}


static
void
testMultipleTracksSingleClusterSimilarity(Session& session)
{
	std::list<ScopedTrack> tracks;
	ScopedClusterType clusterType {session, "MyClusterType"};
	ScopedCluster cluster {session, clusterType.lockAndGet(), "MyClusterType"};

	for (std::size_t i {}; i < 10; ++i)
	{
		tracks.emplace_back(session, "MyTrack" + std::to_string(i));

		{
			auto transaction {session.createUniqueTransaction()};
			cluster.get().modify()->addTrack(tracks.back().get());
		}
	}

	{
		auto transaction {session.createSharedTransaction()};

		const auto similarTracks {Track::getSimilarTracks(session, {tracks.front().getId()})};
		CHECK(similarTracks.size() == tracks.size() - 1);
		for (auto similarTrack : similarTracks)
			CHECK(std::find_if(std::next(std::cbegin(tracks), 1), std::cend(tracks), [&](const ScopedTrack& track) { return similarTrack.id() == track.getId(); }) != std::cend(tracks));
	}
}

static
void
testMultipleTracksMultipleClustersSimilarity(Session& session)
{
	std::list<ScopedTrack> tracks;
	ScopedClusterType clusterType {session, "MyClusterType"};
	ScopedCluster cluster1 {session, clusterType.lockAndGet(), "MyCluster1"};
	ScopedCluster cluster2 {session, clusterType.lockAndGet(), "MyCluster2"};

	for (std::size_t i {}; i < 5; ++i)
	{
		tracks.emplace_back(session, "MyTrack" + std::to_string(i));

		{
			auto transaction {session.createUniqueTransaction()};
			cluster1.get().modify()->addTrack(tracks.back().get());
		}
	}

	for (std::size_t i {5}; i < 10; ++i)
	{
		tracks.emplace_back(session, "MyTrack" + std::to_string(i));

		{
			auto transaction {session.createUniqueTransaction()};
			cluster1.get().modify()->addTrack(tracks.back().get());
			cluster2.get().modify()->addTrack(tracks.back().get());
		}
	}

	{
		auto transaction {session.createSharedTransaction()};

		{
			auto similarTracks {Track::getSimilarTracks(session, {tracks.back().getId()}, 0, 4)};
			CHECK(similarTracks.size() == 4);
			for (auto similarTrack : similarTracks)
				CHECK(std::find_if(std::next(std::cbegin(tracks), 5), std::next(std::cend(tracks), -1), [&](const ScopedTrack& track) { return similarTrack.id() == track.getId(); }) != std::cend(tracks));
		}

		{
			auto similarTracks {Track::getSimilarTracks(session, {tracks.front().getId()})};
			CHECK(similarTracks.size() == tracks.size() - 1);
			for (auto similarTrack : similarTracks)
				CHECK(std::find_if(std::next(std::cbegin(tracks), 1), std::cend(tracks), [&](const ScopedTrack& track) { return similarTrack.id() == track.getId(); }) != std::cend(tracks));
		}
	}
}

static
void
testSingleTrackSingleReleaseSingleCluster(Session& session)
{
	ScopedTrack track {session, "MyTrackFile"};
	ScopedRelease release {session, "MyRelease"};
	ScopedClusterType clusterType {session, "MyClusterType"};
	ScopedCluster cluster {session, clusterType .lockAndGet(), "MyCluster"};

	{
		auto transaction {session.createUniqueTransaction()};

		track.get().modify()->setRelease(release.get());
		cluster.get().modify()->addTrack(track.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		CHECK(Cluster::getAllOrphans(session).empty());
		CHECK(Release::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto releases {Release::getByClusters(session, {cluster.getId()})};
		CHECK(releases.size() == 1);
		CHECK(releases.front().id() == release.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};

		CHECK(cluster->getReleasesCount() == 1);
		CHECK(cluster->getTracksCount() == 1);
	}
}

static
void
testSingleTrackSingleArtistMultiClusters(Session& session)
{
	ScopedTrack track {session, "MyTrackFile"};
	ScopedArtist artist {session, "MyArtist"};
	ScopedClusterType clusterType {session, "MyType"};
	ScopedCluster cluster1 {session, clusterType.lockAndGet(), "Cluster1"};
	ScopedCluster cluster2 {session, clusterType.lockAndGet(), "Cluster2"};
	ScopedCluster cluster3 {session, clusterType.lockAndGet(), "Cluster3"};
	{
		auto transaction {session.createUniqueTransaction()};

		auto trackArtistLink {TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLink::Type::Artist)};
		cluster1.get().modify()->addTrack(track.get());
	}

	{
		auto transaction {session.createSharedTransaction()};
		CHECK(ClusterType::getAllOrphans(session).empty());
		CHECK(Cluster::getAllOrphans(session).size() == 2);
		CHECK(Release::getAllOrphans(session).empty());
		CHECK(Artist::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};
		CHECK(track->getClusters().size() == 1);
		CHECK(track->getClusterIds().size() == 1);
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {Artist::getByClusters(session, {cluster1.getId()})};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist.getId());

		CHECK(Artist::getByClusters(session, {cluster2.getId()}).empty());
		CHECK(Artist::getByClusters(session, {cluster3.getId()}).empty());

		cluster2.get().modify()->addTrack(track.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {Artist::getByClusters(session, {cluster1.getId()})};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist.getId());

		artists = Artist::getByClusters(session, {cluster2.getId()});
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist.getId());

		artists = Artist::getByClusters(session, {cluster1.getId(), cluster2.getId()});
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist.getId());

		CHECK(Artist::getByClusters(session, {cluster3.getId()}).empty());
	}
}

static
void
testSingleTrackSingleArtistMultiRolesMultiClusters(Session& session)
{
	ScopedTrack track {session, "MyTrackFile"};
	ScopedArtist artist {session, "MyArtist"};
	ScopedClusterType clusterType {session, "MyType"};
	ScopedCluster cluster {session, clusterType.lockAndGet(), "MyCluster"};

	{
		auto transaction {session.createUniqueTransaction()};

		TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLink::Type::Artist);
		TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLink::Type::ReleaseArtist);
		cluster.get().modify()->addTrack(track.get());
	}

	{
		auto transaction {session.createSharedTransaction()};
		CHECK(Cluster::getAllOrphans(session).empty());
		CHECK(Release::getAllOrphans(session).empty());
		CHECK(Artist::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {Artist::getByClusters(session, {cluster.getId()})};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist.getId());
	}
}

static
void
testMultiTracksSingleArtistMultiClusters(Session& session)
{
	constexpr std::size_t nbTracks {10};
	constexpr std::size_t nbClusters {5};

	std::list<ScopedTrack> tracks;
	std::list<ScopedCluster> clusters;
	ScopedArtist artist {session, "MyArtist"};
	ScopedClusterType clusterType {session, "MyType"};

	for (std::size_t i {}; i < nbClusters; ++i)
		clusters.emplace_back(session, clusterType.lockAndGet(), "MyCluster" + std::to_string(i));

	for (std::size_t i {}; i < nbTracks ; ++i)
	{
		tracks.emplace_back(session, "MyTrackFile" + std::to_string(i));

		auto transaction {session.createUniqueTransaction()};
		TrackArtistLink::create(session, tracks.back().get(), artist.get(), TrackArtistLink::Type::Artist);

		for (auto& cluster : clusters)
			cluster.get().modify()->addTrack(tracks.back().get());
	}

	{
		auto transaction {session.createSharedTransaction()};
		CHECK(Cluster::getAllOrphans(session).empty());
		CHECK(Artist::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		std::set<IdType> clusterIds;
		std::transform(std::cbegin(clusters), std::cend(clusters), std::inserter(clusterIds, std::begin(clusterIds)), [](const ScopedCluster& cluster) { return cluster.getId(); });

		auto artists {Artist::getByClusters(session, clusterIds)};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist.getId());
	}
}

static
void
testMultiTracksSingleArtistSingleRelease(Session& session)
{
	constexpr std::size_t nbTracks {10};
	std::list<ScopedTrack> tracks;
	ScopedArtist artist {session, "MyArtst"};
	ScopedRelease release {session, "MyRelease"};

	for (std::size_t i {}; i < nbTracks; ++i)
	{
		tracks.emplace_back(session, "MyTrack" + std::to_string(i));

		auto transaction {session.createUniqueTransaction()};

		TrackArtistLink::create(session, tracks.back().get(), artist.get(), TrackArtistLink::Type::Artist);
		tracks.back().get().modify()->setRelease(release.get());
	}

	{
		auto transaction {session.createSharedTransaction()};
		CHECK(Release::getAllOrphans(session).empty());
		CHECK(Artist::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		CHECK(artist->getReleaseCount() == 1);
		CHECK(artist->getReleases().size() == 1);
		CHECK(artist->getReleases().front().id() == release.getId());

		CHECK(release->getTracks().size() == nbTracks);
	}

}

static
void
testSingleTrackSingleReleaseSingleArtist(Session& session)
{
	ScopedTrack track {session, "MyTrack"};
	ScopedRelease release {session, "MyRelease"};
	ScopedArtist artist {session, "MyArtist"};

	{
		auto transaction {session.createUniqueTransaction()};

		auto trackArtistLink {TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLink::Type::Artist)};
		track.get().modify()->setRelease(release.get());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto releases {artist->getReleases()};
		CHECK(releases.size() == 1);
		CHECK(releases.front().id() == release.getId());

		CHECK(artist->getReleaseCount() == 1);

		auto artists {release->getArtists()};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist.getId());
	}
}

static
void
testSingleTrackSingleReleaseSingleArtistSingleCluster(Session& session)
{
	ScopedTrack track {session, "MyTrack"};
	ScopedRelease release {session, "MyRelease"};
	ScopedArtist artist {session, "MyArtist"};
	ScopedClusterType clusterType {session, "MyType"};
	ScopedCluster cluster {session, clusterType.lockAndGet(), "MyCluster"};

	{
		auto transaction {session.createUniqueTransaction()};

		TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLink::Type::Artist);
		track.get().modify()->setRelease(release.get());
		cluster.get().modify()->addTrack(track.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		CHECK(Cluster::getAllOrphans(session).empty());
		CHECK(ClusterType::getAllOrphans(session).empty());
		CHECK(Artist::getAllOrphans(session).empty());
		CHECK(Release::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {Artist::getByClusters(session, {cluster.getId()})};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist.getId());

		auto releases {artist->getReleases()};
		CHECK(releases.size() == 1);
		CHECK(releases.front().id() == release.getId());

		releases = artist->getReleases({cluster.getId()});
		CHECK(releases.size() == 1);
		CHECK(releases.front().id() == release.getId());
	}
}

static
void
testSingleTrackSingleReleaseSingleArtistMultiClusters(Session& session)
{
	ScopedTrack track {session, "MyTrack"};
	ScopedRelease release {session, "MyRelease"};
	ScopedArtist artist {session, "MyArtist"};
	ScopedClusterType clusterType {session, "MyClusterType"};
	ScopedCluster cluster1 {session, clusterType.lockAndGet(), "MyCluster1"};
	ScopedCluster cluster2 {session, clusterType.lockAndGet(), "MyCluster2"};

	{
		auto transaction {session.createUniqueTransaction()};

		auto trackArtistLink {TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLink::Type::Artist)};
		track.get().modify()->setRelease(release.get());
		cluster1.get().modify()->addTrack(track.get());
		cluster2.get().modify()->addTrack(track.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto releases {artist->getReleases()};
		CHECK(releases.size() == 1);
		CHECK(releases.front().id() == release.getId());

		releases = artist->getReleases({cluster1.getId(), cluster2.getId()});
		CHECK(releases.size() == 1);
		CHECK(releases.front().id() == release.getId());
	}
}

static
void
testSingleUser(Session& session)
{
	ScopedUser user {session, "MyUser", User::PasswordHash {}};

	{
		auto transaction {session.createSharedTransaction()};

		CHECK(user->getPlayedTrackList(session)->getCount() == 0);
		CHECK(user->getPlayedTrackList(session)->getTopTracks(1).empty());
		CHECK(user->getPlayedTrackList(session)->getTopArtists(1).empty());
		CHECK(user->getPlayedTrackList(session)->getTopReleases(1).empty());
		CHECK(user->getQueuedTrackList(session)->getCount() == 0);
	}
}

static
void
testSingleStarredArtist(Session& session)
{
	ScopedArtist artist {session, "MyArtist"};
	ScopedUser user {session, "MyUser", User::PasswordHash {}};

	{
		auto transaction {session.createSharedTransaction()};

		user.get().modify()->starArtist(artist.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {user->getStarredArtists()};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist.getId());

		CHECK(user->hasStarredArtist(artist.get()));
	}
}

static
void
testSingleStarredRelease(Session& session)
{
	ScopedRelease release {session, "MyRelease"};
	ScopedUser user {session, "MyUser", User::PasswordHash {}};

	{
		auto transaction {session.createUniqueTransaction()};

		user.get().modify()->starRelease(release.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto releases {user->getStarredReleases()};
		CHECK(releases.size() == 1);
		CHECK(releases.front().id() == release.getId());

		CHECK(user->hasStarredRelease(release.get()));
	}
}

static
void
testSingleStarredTrack(Session& session)
{
	ScopedTrack track {session, "MyTrack"};
	ScopedUser user {session, "MyUser", User::PasswordHash {}};

	{
		auto transaction {session.createUniqueTransaction()};

		user.get().modify()->starTrack(track.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto tracks {user->getStarredTracks()};
		CHECK(tracks.size() == 1);
		CHECK(tracks.front().id() == track.getId());

		CHECK(user->hasStarredTrack(track.get()));
	}
}

static
void
testSingleTrackList(Session& session)
{
	ScopedUser user {session, "MyUser", User::PasswordHash {}};
	ScopedTrackList trackList {session, "MytrackList", TrackList::Type::Playlist, false, user.lockAndGet()};

	{
		auto transaction {session.createSharedTransaction()};

		auto trackLists {TrackList::getAll(session, user.get(), TrackList::Type::Playlist)};
		CHECK(trackLists.size() == 1);
		CHECK(trackLists.front().id() == trackList.getId());
	}

}

static
void
testSingleTrackListMultipleTrack(Session& session)
{
	ScopedUser user {session, "MyUser", User::PasswordHash {}};
	ScopedTrackList trackList {session, "MytrackList", TrackList::Type::Playlist, false, user.lockAndGet()};
	std::list<ScopedTrack> tracks;

	for (std::size_t i {}; i < 10; ++i)
	{
		tracks.emplace_back(session, "MyTrack" + std::to_string(i));

		auto transaction {session.createUniqueTransaction()};
		TrackListEntry::create(session, tracks.back().get(), trackList.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		CHECK(trackList->getCount() == tracks.size());
		const auto trackIds {trackList->getTrackIds()};
		for (auto trackId : trackIds)
			CHECK(std::any_of(std::cbegin(tracks), std::cend(tracks), [trackId](const ScopedTrack& track) { return track.getId() == trackId; }));
	}
}

static
void
testSingleTrackListMultipleTrackSingleCluster(Session& session)
{
	ScopedUser user {session, "MyUser", User::PasswordHash {}};
	ScopedTrackList trackList {session, "MyTrackList", TrackList::Type::Playlist, false, user.lockAndGet()};
	ScopedClusterType clusterType {session, "MyClusterType"};
	ScopedCluster cluster {session, clusterType.lockAndGet(), "MyCluster"};
	std::list<ScopedTrack> tracks;

	for (std::size_t i {}; i < 20; ++i)
	{
		tracks.emplace_back(session, "MyTrack" + std::to_string(i));

		auto transaction {session.createUniqueTransaction()};

		if (i < 5)
			TrackListEntry::create(session, tracks.back().get(), trackList.get());

		if (i < 10)
			cluster.get().modify()->addTrack(tracks.back().get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		const auto similarTracks {trackList->getSimilarTracks()};
		CHECK(similarTracks.size() == 5);

		for (auto similarTrack : similarTracks)
			CHECK(std::any_of(std::next(std::cbegin(tracks), 5), std::cend(tracks), [similarTrack](const ScopedTrack& track) { return track.getId() == similarTrack.id(); }));
	}
}

static
void
testSingleTrackListMultipleTrackMultiClusters(Session& session)
{
	ScopedUser user {session, "MyUser", User::PasswordHash {}};
	ScopedTrackList trackList {session, "MyTrackList", TrackList::Type::Playlist, false, user.lockAndGet()};
	ScopedClusterType clusterType {session, "MyClusterType"};
	ScopedCluster cluster1 {session, clusterType.lockAndGet(), "MyCluster1"};
	ScopedCluster cluster2 {session, clusterType.lockAndGet(), "MyCluster2"};
	std::list<ScopedTrack> tracks;

	for (std::size_t i {}; i < 20; ++i)
	{
		tracks.emplace_back(session, "MyTrack" + std::to_string(i));

		auto transaction {session.createUniqueTransaction()};

		if (i < 5)
			TrackListEntry::create(session, tracks.back().get(), trackList.get());

		if (i < 10)
		{
			cluster1.get().modify()->addTrack(tracks.back().get());
			cluster2.get().modify()->addTrack(tracks.back().get());
		}
		else if (i < 15)
		{
			cluster1.get().modify()->addTrack(tracks.back().get());
		}
	}

	{
		auto transaction {session.createSharedTransaction()};

		{
			const auto similarTracks {trackList->getSimilarTracks(0, 5)};
			CHECK(similarTracks.size() == 5);

			for (auto similarTrack : similarTracks)
				CHECK(std::any_of(std::next(std::cbegin(tracks), 5), std::next(std::cbegin(tracks), 10), [similarTrack](const ScopedTrack& track) { return track.getId() == similarTrack.id(); }));
		}

		{
			const auto similarTracks {trackList->getSimilarTracks(5, 10)};
			CHECK(similarTracks.size() == 5);

			for (auto similarTrack : similarTracks)
				CHECK(std::any_of(std::next(std::cbegin(tracks), 10), std::next(std::cbegin(tracks), 15), [similarTrack](const ScopedTrack& track) { return track.getId() == similarTrack.id(); }));
		}

		CHECK(trackList->getSimilarTracks(10, 10).empty());

	}
}

static
void
testMultipleTracksMultipleArtistsMultiClusters(Session& session)
{
	ScopedArtist artist1 {session, "MyArtist1"};
	ScopedArtist artist2 {session, "MyArtist2"};
	ScopedArtist artist3 {session, "MyArtist3"};
	ScopedClusterType clusterType {session, "MyClusterType"};
	ScopedCluster cluster1 {session, clusterType.lockAndGet(), "MyCluster1"};
	ScopedCluster cluster2 {session, clusterType.lockAndGet(), "MyCluster2"};

	{
		auto transaction {session.createSharedTransaction()};
		CHECK(artist1->getSimilarArtists().empty());
		CHECK(artist2->getSimilarArtists().empty());
		CHECK(artist3->getSimilarArtists().empty());
	}

	std::list<ScopedTrack> tracks;
	for (std::size_t i {}; i < 10; ++i)
	{
		tracks.emplace_back(session, "MyTrack" + std::to_string(i));

		auto transaction {session.createUniqueTransaction()};

		if (i < 5)
			TrackArtistLink::create(session, tracks.back().get(), artist1.get(), TrackArtistLink::Type::Artist);
		else
		{
			TrackArtistLink::create(session, tracks.back().get(), artist2.get(), TrackArtistLink::Type::Artist);
			cluster2.get().modify()->addTrack(tracks.back().get());
		}

		cluster1.get().modify()->addTrack(tracks.back().get());
	}

	tracks.emplace_back(session, "MyTrack" + std::to_string(tracks.size()));
	{
		auto transaction {session.createUniqueTransaction()};
		TrackArtistLink::create(session, tracks.back().get(), artist3.get(), TrackArtistLink::Type::Artist);
		cluster2.get().modify()->addTrack(tracks.back().get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		{
			auto artists {artist1->getSimilarArtists()};
			CHECK(artists.size() == 1);
			CHECK(artists.front().id() == artist2.getId());
		}

		{
			auto artists {artist2->getSimilarArtists()};
			CHECK(artists.size() == 2);
			CHECK(artists[0].id() == artist1.getId());
			CHECK(artists[1].id() == artist3.getId());
		}
	}
}

static
void
testMultipleTracksMultipleReleasesMultiClusters(Session& session)
{
	ScopedRelease release1 {session, "MyRelease1"};
	ScopedRelease release2 {session, "MyRelease2"};
	ScopedRelease release3 {session, "MyRelease3"};
	ScopedClusterType clusterType {session, "MyClusterType"};
	ScopedCluster cluster1 {session, clusterType.lockAndGet(), "MyCluster1"};
	ScopedCluster cluster2 {session, clusterType.lockAndGet(), "MyCluster2"};

	{
		auto transaction {session.createSharedTransaction()};
		CHECK(release1->getSimilarReleases().empty());
		CHECK(release2->getSimilarReleases().empty());
		CHECK(release3->getSimilarReleases().empty());
	}

	std::list<ScopedTrack> tracks;
	for (std::size_t i {}; i < 10; ++i)
	{
		tracks.emplace_back(session, "MyTrack" + std::to_string(i));

		auto transaction {session.createUniqueTransaction()};

		if (i < 5)
			tracks.back().get().modify()->setRelease(release1.get());
		else
		{
			tracks.back().get().modify()->setRelease(release2.get());
			cluster2.get().modify()->addTrack(tracks.back().get());
		}

		cluster1.get().modify()->addTrack(tracks.back().get());
	}

	tracks.emplace_back(session, "MyTrack" + std::to_string(tracks.size()));
	{
		auto transaction {session.createUniqueTransaction()};
		tracks.back().get().modify()->setRelease(release3.get());
		cluster2.get().modify()->addTrack(tracks.back().get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		{
			auto releases {release1->getSimilarReleases()};
			CHECK(releases.size() == 1);
			CHECK(releases.front().id() == release2.getId());
		}

		{
			auto releases {release2->getSimilarReleases()};
			CHECK(releases.size() == 2);
			CHECK(releases[0].id() == release1.getId());
			CHECK(releases[1].id() == release3.getId());
		}
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
	CHECK(TrackList::getAll(session).empty());
	CHECK(User::getAll(session).empty());
}

int main()
{

	try
	{
		// log to stdout
		ServiceProvider<Logger>::create<StreamLogger>(std::cout);

		const std::filesystem::path tmpFile {std::tmpnam(nullptr)};
		ScopedFileDeleter tmpFileDeleter {tmpFile};

		std::cout << "Database test file: '" << tmpFile.string() << "'" << std::endl;

		for (std::size_t i = 0; i < 2; ++i)
		{
			Database::Db db {tmpFile};
			Database::Session session {db};
			session.prepareTables();

			auto runTest = [&session](const std::string& name, std::function<void(Session&)> testFunc)
			{
				std::cout << "Running test '" << name << "'..." << std::endl;
				testFunc(session);
				testDatabaseEmpty(session);
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
			RUN_TEST(testMultipleTracksSingleCluster);

			RUN_TEST(testMultipleTracksSingleClusterSimilarity);
			RUN_TEST(testMultipleTracksMultipleClustersSimilarity);

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

			RUN_TEST(testSingleTrackList);
			RUN_TEST(testSingleTrackListMultipleTrack);
			RUN_TEST(testSingleTrackListMultipleTrackSingleCluster);
			RUN_TEST(testSingleTrackListMultipleTrackMultiClusters);
			RUN_TEST(testMultipleTracksMultipleArtistsMultiClusters);
			RUN_TEST(testMultipleTracksMultipleReleasesMultiClusters);
		}
	}
	catch (std::exception& e)
	{
		std::cerr << "Caught exception: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
