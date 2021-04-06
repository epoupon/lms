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
#include "database/TrackArtistLink.hpp"
#include "database/TrackBookmark.hpp"
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
		catch (Wt::Dbo::Exception& e) \
		{ \
			std::cerr << "Caught DBO exception: " <<e.code() << std::endl; \
			throw; \
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
using ScopedTrackBookmark = ScopedEntity<TrackBookmark>;
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
	{
		auto transaction {session.createSharedTransaction()};
		CHECK(Track::getCount(session) == 0);
	}

	ScopedTrack track {session, "MyTrackFile"};

	{
		auto transaction {session.createSharedTransaction()};

		CHECK(Track::getAll(session).size() == 1);
		CHECK(Track::getCount(session) == 1);

	}
}

static
void
testSingleArtist(Session& session)
{
	ScopedArtist artist {session, "MyArtist"};

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {Artist::getAll(session, Artist::SortMethod::ByName)};
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

		CHECK(release->getDuration() == std::chrono::seconds {0});
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

			clusterTypes = ClusterType::getAllUsed(session);
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

		CHECK(ClusterType::getAllUsed(session).empty());
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

		TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
	}

	{
		auto transaction {session.createSharedTransaction()};
		CHECK(Artist::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {track->getArtists({TrackArtistLinkType::Artist})};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist.getId());

		CHECK(artist->getReleaseCount() == 0);

		CHECK(track->getArtistLinks().size() == 1);
		auto artistLink {track->getArtistLinks().front()};
		CHECK(artistLink->getTrack().id() == track.getId());
		CHECK(artistLink->getArtist().id() == artist.getId());

		CHECK(track->getArtists({TrackArtistLinkType::Artist}).size() == 1);
		CHECK(track->getArtists({TrackArtistLinkType::ReleaseArtist}).empty());
		CHECK(track->getArtists({}).size() == 1);
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto tracks {artist->getTracks()};
		CHECK(tracks.size() == 1);
		CHECK(tracks.front().id() == track.getId());

		CHECK(artist->getTracks(TrackArtistLinkType::ReleaseArtist).empty());
		CHECK(artist->getTracks(TrackArtistLinkType::Artist).size() == 1);
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

		TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
		TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::ReleaseArtist);
		TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Writer);
	}

	{
		auto transaction {session.createSharedTransaction()};
		CHECK(Artist::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};
		bool hasMore{};
		CHECK(Artist::getByFilter(session, {}, {}, std::nullopt, Artist::SortMethod::ByName, std::nullopt, hasMore).size() == 1);
		CHECK(Artist::getByFilter(session, {}, {}, TrackArtistLinkType::Artist, Artist::SortMethod::ByName, std::nullopt, hasMore).size() == 1);
		CHECK(Artist::getByFilter(session, {}, {}, TrackArtistLinkType::ReleaseArtist, Artist::SortMethod::ByName, std::nullopt, hasMore).size() == 1);
		CHECK(Artist::getByFilter(session, {}, {}, TrackArtistLinkType::Writer, Artist::SortMethod::ByName, std::nullopt, hasMore).size() == 1);
		CHECK(Artist::getByFilter(session, {}, {}, TrackArtistLinkType::Composer, Artist::SortMethod::ByName, std::nullopt, hasMore).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {track->getArtists({TrackArtistLinkType::Artist})};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist.getId());

		artists = track->getArtists({TrackArtistLinkType::ReleaseArtist});
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist.getId());

		CHECK(track->getArtistLinks().size() == 3);

		CHECK(artist->getTracks().size() == 1);
		CHECK(artist->getTracks({TrackArtistLinkType::ReleaseArtist}).size() == 1);
		CHECK(artist->getTracks({TrackArtistLinkType::Artist}).size() == 1);
		CHECK(artist->getTracks({TrackArtistLinkType::Writer}).size() == 1);
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

		TrackArtistLink::create(session, track.get(), artist1.get(), TrackArtistLinkType::Artist);
		TrackArtistLink::create(session, track.get(), artist2.get(), TrackArtistLinkType::Artist);
	}

	{
		auto transaction {session.createSharedTransaction()};
		CHECK(Artist::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {track->getArtists({TrackArtistLinkType::Artist})};
		CHECK(artists.size() == 2);
		CHECK((artists[0].id() == artist1.getId() && artists[1].id() == artist2.getId())
			|| (artists[0].id() == artist2.getId() && artists[1].id() == artist1.getId()));

		CHECK(track->getArtists({}).size() == 2);
		CHECK(track->getArtists({TrackArtistLinkType::Artist}).size() == 2);
		CHECK(track->getArtists({TrackArtistLinkType::ReleaseArtist}).empty());
		CHECK(Artist::getAll(session, Artist::SortMethod::ByName).size() == 2);
		CHECK(Artist::getAllIds(session).size() == 2);
	}

	{
		auto transaction {session.createUniqueTransaction()};

		CHECK(artist1->getTracks().front() == track.get());
		CHECK(artist2->getTracks().front() == track.get());

		CHECK(artist1->getTracks(TrackArtistLinkType::ReleaseArtist).empty());
		CHECK(artist1->getTracks(TrackArtistLinkType::Artist).size() == 1);
		CHECK(artist2->getTracks(TrackArtistLinkType::ReleaseArtist).empty());
		CHECK(artist2->getTracks(TrackArtistLinkType::Artist).size() == 1);
	}
}

static
void
testSingleArtistSearchByName(Session& session)
{
	ScopedArtist artist {session, "AAA"};
	ScopedTrack track {session, "MyTrack"}; // filters does not work on orphans

	{
		auto transaction {session.createUniqueTransaction()};
		artist.get().modify()->setSortName("ZZZ");
		TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool more {};
		CHECK(Artist::getByFilter(session, {}, {"N"}, std::nullopt, Artist::SortMethod::ByName, std::nullopt, more).empty());

		const auto artistsByAAA {Artist::Artist::getByFilter(session, {}, {"A"}, std::nullopt, Artist::SortMethod::ByName, std::nullopt, more)};
		CHECK(artistsByAAA.size() == 1);
		CHECK(artistsByAAA.front().id() == artist.getId());

		const auto artistsByZZZ {Artist::Artist::getByFilter(session, {}, {"Z"}, std::nullopt, Artist::SortMethod::ByName, std::nullopt, more)};
		CHECK(artistsByZZZ.size() == 1);
		CHECK(artistsByZZZ.front().id() == artist.getId());

		CHECK(Artist::getByName(session, "NNN").empty());
	}
}

static
void
testMultiArtistsSortMethod(Session& session)
{
	ScopedArtist artistA {session, "artistA"};
	ScopedArtist artistB {session, "artistB"};

	{
		auto transaction {session.createUniqueTransaction()};

		artistA.get().modify()->setSortName("sortNameB");
		artistB.get().modify()->setSortName("sortNameA");
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto allArtistsByName {Artist::getAll(session, Artist::SortMethod::ByName)};
		auto allArtistsBySortName {Artist::getAll(session, Artist::SortMethod::BySortName)};

		CHECK(allArtistsByName.size() == 2);
		CHECK(allArtistsByName.front().id() == artistA.getId());
		CHECK(allArtistsByName.back().id() == artistB.getId());

		CHECK(allArtistsBySortName.size() == 2);
		CHECK(allArtistsBySortName.front().id() == artistB.getId());
		CHECK(allArtistsBySortName.back().id() == artistA.getId());
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
testMultiTracksSingleReleaseTotalDiscTrack(Session& session)
{
	ScopedRelease release1 {session, "MyRelease"};

	{
		auto transaction {session.createSharedTransaction()};

		CHECK(!release1->getTotalTrack());
		CHECK(!release1->getTotalDisc());
	}

	ScopedTrack track1 {session, "MyTrack"};
	{
		auto transaction {session.createUniqueTransaction()};

		track1.get().modify()->setRelease(release1.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		CHECK(!release1->getTotalTrack());
		CHECK(!release1->getTotalDisc());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		track1.get().modify()->setTotalTrack(36);
		track1.get().modify()->setTotalDisc(6);
	}

	{
		auto transaction {session.createSharedTransaction()};

		CHECK(release1->getTotalTrack() && *release1->getTotalTrack() == 36);
		CHECK(release1->getTotalDisc() && *release1->getTotalDisc() == 6);
	}

	ScopedTrack track2 {session, "MyTrack2"};
	{
		auto transaction {session.createUniqueTransaction()};

		track2.get().modify()->setRelease(release1.get());
		track2.get().modify()->setTotalTrack(37);
		track2.get().modify()->setTotalDisc(67);
	}

	{
		auto transaction {session.createSharedTransaction()};

		CHECK(release1->getTotalTrack() && *release1->getTotalTrack() == 37);
		CHECK(release1->getTotalDisc() && *release1->getTotalDisc() == 67);
	}

	ScopedRelease release2 {session, "MyRelease2"};
	{
		auto transaction {session.createSharedTransaction()};

		CHECK(!release2->getTotalTrack());
		CHECK(!release2->getTotalDisc());
	}

	ScopedTrack track3 {session, "MyTrack3"};
	{
		auto transaction {session.createUniqueTransaction()};

		track3.get().modify()->setRelease(release2.get());
		track3.get().modify()->setTotalTrack(7);
		track3.get().modify()->setTotalDisc(5);
	}
	{
		auto transaction {session.createSharedTransaction()};

		CHECK(release1->getTotalTrack() && *release1->getTotalTrack() == 37);
		CHECK(release1->getTotalDisc() && *release1->getTotalDisc() == 67);
		CHECK(release2->getTotalTrack() && *release2->getTotalTrack() == 7);
		CHECK(release2->getTotalDisc() && *release2->getTotalDisc() == 5);
	}
}

static
void
testMultiTracksSingleReleaseFirstTrack(Session& session)
{
	ScopedRelease release1 {session, "MyRelease1"};
	ScopedRelease release2 {session, "MyRelease2"};

	ScopedTrack track1A {session, "MyTrack1A"};
	ScopedTrack track1B {session, "MyTrack1B"};
	ScopedTrack track2A {session, "MyTrack2A"};
	ScopedTrack track2B {session, "MyTrack2B"};

	{
		auto transaction {session.createSharedTransaction()};

		CHECK(!release1->getFirstTrack());
		CHECK(!release2->getFirstTrack());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		track1A.get().modify()->setRelease(release1.get());
		track1B.get().modify()->setRelease(release1.get());
		track2A.get().modify()->setRelease(release2.get());
		track2B.get().modify()->setRelease(release2.get());

		track1A.get().modify()->setTrackNumber(1);
		track1B.get().modify()->setTrackNumber(2);

		track2A.get().modify()->setDiscNumber(2);
		track2A.get().modify()->setTrackNumber(1);
		track2B.get().modify()->setTrackNumber(2);
		track2B.get().modify()->setDiscNumber(1);
	}

	{
		auto transaction {session.createSharedTransaction()};

		CHECK(release1->getFirstTrack());
		CHECK(release2->getFirstTrack());

		CHECK(release1->getFirstTrack().id() == track1A.getId());
		CHECK(release2->getFirstTrack().id() == track2B.getId());
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
		auto transaction {session.createSharedTransaction()};
		CHECK(Track::getAllIdsWithClusters(session).empty());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		cluster1.get().modify()->addTrack(track.get());
	}

	{
		auto transaction {session.createSharedTransaction()};
		auto tracks {Track::getAllIdsWithClusters(session)};
		CHECK(tracks.size() == 1);
		CHECK(tracks.front() == track.getId());
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
	ScopedCluster cluster {session, clusterType.lockAndGet(), "MyCluster"};

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
testMultipleTracksMultipleClustersTopRelease(Session& session)
{
	ScopedClusterType clusterType {session, "ClusterType"};
	ScopedCluster cluster1 {session, clusterType.lockAndGet(), "Cluster1"};
	ScopedCluster cluster2 {session, clusterType.lockAndGet(), "Cluster2"};
	ScopedCluster cluster3 {session, clusterType.lockAndGet(), "Cluster3"};
	ScopedTrack trackA {session, "TrackA"};
	ScopedTrack trackB {session, "TrackB"};
	ScopedTrack trackC {session, "TrackC"};
	ScopedRelease releaseA {session, "ReleaseA"};
	ScopedRelease releaseB {session, "ReleaseB"};
	ScopedRelease releaseC {session, "ReleaseC"};

	ScopedUser user {session, "MyUser"};
	ScopedTrackList trackList {session, "TrackList", TrackList::Type::Playlist, false, user.lockAndGet()};

	{
		auto transaction {session.createSharedTransaction()};

		CHECK(trackList->getDuration() == std::chrono::seconds {0});
	}

	{
		auto transaction {session.createUniqueTransaction()};

		cluster1.get().modify()->addTrack(trackA.get());
		cluster2.get().modify()->addTrack(trackB.get());
		cluster2.get().modify()->addTrack(trackC.get());
		cluster3.get().modify()->addTrack(trackC.get());

		trackA.get().modify()->setRelease(releaseA.get());
		trackB.get().modify()->setRelease(releaseB.get());
		trackC.get().modify()->setRelease(releaseC.get());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		TrackListEntry::create(session, trackA.get(), trackList.get());
		TrackListEntry::create(session, trackB.get(), trackList.get());
		TrackListEntry::create(session, trackB.get(), trackList.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool hasMore;
		const auto releases{trackList->getTopReleases({}, std::nullopt, hasMore)};
		CHECK(releases.size() == 2);
		CHECK(releases[0].id() == releaseB.getId());
		CHECK(releases[1].id() == releaseA.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};

 		bool hasMore;
		auto releases{trackList->getTopReleases({cluster1.getId()}, std::nullopt, hasMore)};
		CHECK(releases.size() == 1);
		CHECK(releases[0].id() == releaseA.getId());

		releases = trackList->getTopReleases({cluster2.getId()}, std::nullopt, hasMore);
		CHECK(releases.size() == 1);
		CHECK(releases[0].id() == releaseB.getId());

		releases = trackList->getTopReleases({cluster2.getId(), cluster1.getId()}, std::nullopt, hasMore);
		CHECK(releases.empty());

		releases = trackList->getTopReleases({cluster2.getId(), cluster3.getId()}, std::nullopt, hasMore);
		CHECK(releases.empty());
	}


	{
		auto transaction {session.createUniqueTransaction()};

		TrackListEntry::create(session, trackC.get(), trackList.get());
		TrackListEntry::create(session, trackC.get(), trackList.get());
		TrackListEntry::create(session, trackC.get(), trackList.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

 		bool hasMore;
		auto releases {trackList->getTopReleases({cluster2.getId(), cluster3.getId()}, std::nullopt, hasMore)};
		CHECK(releases.size() == 1);
		CHECK(releases[0].id() == releaseC.getId());

		releases = trackList->getTopReleases({cluster2.getId()}, std::nullopt, hasMore);
		CHECK(releases.size() == 2);
		CHECK(releases[0].id() == releaseC.getId());
		CHECK(releases[1].id() == releaseB.getId());
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
		{
			CHECK(std::find_if(std::next(std::cbegin(tracks), 1), std::cend(tracks),
						[&](const ScopedTrack& track)
						{
							return similarTrack.id() == track.getId();
						}) != std::cend(tracks));
		}
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
		auto transaction {session.createSharedTransaction()};
		CHECK(Release::getAllIdsWithClusters(session).empty());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		track.get().modify()->setRelease(release.get());
		cluster.get().modify()->addTrack(track.get());
	}

	{
		auto transaction {session.createSharedTransaction()};
		auto releases {Release::getAllIdsWithClusters(session)};
		CHECK(releases.size() == 1);
		CHECK(releases.front() == release.getId());
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

		auto trackArtistLink {TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist)};
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

		auto artists {Artist::getByClusters(session, {cluster1.getId()}, Artist::SortMethod::ByName)};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist.getId());

		CHECK(Artist::getByClusters(session, {cluster2.getId()}, Artist::SortMethod::ByName).empty());
		CHECK(Artist::getByClusters(session, {cluster3.getId()}, Artist::SortMethod::ByName).empty());

		cluster2.get().modify()->addTrack(track.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {Artist::getByClusters(session, {cluster1.getId()}, Artist::SortMethod::ByName)};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist.getId());

		artists = Artist::getByClusters(session, {cluster2.getId()}, Artist::SortMethod::ByName);
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist.getId());

		artists = Artist::getByClusters(session, {cluster1.getId(), cluster2.getId()}, Artist::SortMethod::ByName);
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist.getId());

		CHECK(Artist::getByClusters(session, {cluster3.getId()}, Artist::SortMethod::ByName).empty());
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

		TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
		TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::ReleaseArtist);
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

		auto artists {Artist::getByClusters(session, {cluster.getId()}, Artist::SortMethod::ByName)};
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
		TrackArtistLink::create(session, tracks.back().get(), artist.get(), TrackArtistLinkType::Artist);

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

		auto artists {Artist::getByClusters(session, clusterIds, Artist::SortMethod::ByName)};
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

		TrackArtistLink::create(session, tracks.back().get(), artist.get(), TrackArtistLinkType::Artist);
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

		auto trackArtistLink {TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist)};
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
		auto transaction {session.createSharedTransaction()};
		CHECK(Artist::getAllIdsWithClusters(session).empty());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
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
		auto artists {Artist::getAllIdsWithClusters(session)};
		CHECK(artists.size() == 1);
		CHECK(artists.front() == artist.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {Artist::getByClusters(session, {cluster.getId()}, Artist::SortMethod::ByName)};
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

		auto trackArtistLink {TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist)};
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
	ScopedUser user {session, "MyUser"};

	{
		auto transaction {session.createSharedTransaction()};

		CHECK(user->getQueuedTrackList(session)->getCount() == 0);
	}
}

static
void
testSingleStarredArtist(Session& session)
{
	ScopedArtist artist {session, "MyArtist"};
	ScopedTrack track {session, "MyTrack"};
	ScopedUser user {session, "MyUser"};

	{
		auto transaction {session.createUniqueTransaction()};

		auto trackArtistLink {TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist)};
		user.get().modify()->starArtist(artist.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		CHECK(user->hasStarredArtist(artist.get()));

		bool hasMore {};
		auto artists {Artist::getStarred(session, user.get(), {}, std::nullopt, Artist::SortMethod::BySortName, std::nullopt, hasMore)};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist.getId());
		CHECK(hasMore == false);
	}
}

static
void
testSingleStarredRelease(Session& session)
{
	ScopedRelease release {session, "MyRelease"};
	ScopedTrack track {session, "MyTrack"};
	ScopedUser user {session, "MyUser"};

	{
		auto transaction {session.createUniqueTransaction()};

		track.get().modify()->setRelease(release.get());
		user.get().modify()->starRelease(release.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		CHECK(user->hasStarredRelease(release.get()));

		bool hasMore {};
		auto releases {Release::getStarred(session, user.get(), {}, std::nullopt, hasMore)};
		CHECK(releases.size() == 1);
		CHECK(releases.front().id() == release.getId());
		CHECK(hasMore == false);
	}
}

static
void
testSingleStarredTrack(Session& session)
{
	ScopedTrack track {session, "MyTrack"};
	ScopedUser user {session, "MyUser"};

	{
		auto transaction {session.createUniqueTransaction()};

		user.get().modify()->starTrack(track.get());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		CHECK(user->hasStarredTrack(track.get()));

		bool hasMore {};
		auto tracks {Track::getStarred(session, user.get(), {}, std::nullopt, hasMore)};
		CHECK(tracks.size() == 1);
		CHECK(tracks.front().id() == track.getId());
		CHECK(hasMore == false);
	}
}

static
void
testSingleTrackList(Session& session)
{
	ScopedUser user {session, "MyUser"};
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
	ScopedUser user {session, "MyUser"};
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

void
testSingleTrackListMultipleTrackDateTime(Session& session)
{
	ScopedUser user {session, "MyUser"};
	ScopedTrackList trackList {session, "MytrackList", TrackList::Type::Playlist, false, user.lockAndGet()};
	ScopedTrack track1 {session, "MyTrack1"};
	ScopedTrack track2 {session, "MyTrack2"};
	ScopedTrack track3 {session, "MyTrack3"};

	{
		Wt::WDateTime now {Wt::WDateTime::currentDateTime()};
		auto transaction {session.createUniqueTransaction()};
		TrackListEntry::create(session, track1.get(), trackList.get(), now);
		TrackListEntry::create(session, track2.get(), trackList.get(), now.addSecs(-1));
		TrackListEntry::create(session, track3.get(), trackList.get(), now.addSecs(1));
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults;
		const auto tracks {trackList.get()->getTracksReverse({}, std::nullopt, moreResults)};
		CHECK(tracks.size() == 3);
		CHECK(tracks.front().id() == track3.getId());
		CHECK(tracks.back().id() == track2.getId());
	}
}

static
void
testSingleTrackListMultipleTrackSingleCluster(Session& session)
{
	ScopedUser user {session, "MyUser"};
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
	ScopedUser user {session, "MyUser"};
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
testSingleTrackListMultipleTrackRecentlyPlayed(Session& session)
{
	ScopedUser user {session, "MyUser"};
	ScopedTrackList trackList {session, "MyTrackList", TrackList::Type::Playlist, false, user.lockAndGet()};
	ScopedClusterType clusterType {session, "MyClusterType"};
	ScopedTrack track1 {session, "MyTrack1"};
	ScopedTrack track2 {session, "MyTrack1"};
	ScopedArtist artist1 {session, "MyArtist1"};
	ScopedArtist artist2 {session, "MyArtist2"};
	ScopedRelease release1 {session, "MyRelease1"};
	ScopedRelease release2 {session, "MyRelease2"};

	const Wt::WDateTime now {Wt::WDateTime::currentDateTime()};

	{
		auto transaction {session.createUniqueTransaction()};

		track1.get().modify()->setRelease(release1.get());
		track2.get().modify()->setRelease(release2.get());
		TrackArtistLink::create(session, track1.get(), artist1.get(), TrackArtistLinkType::Artist);
		TrackArtistLink::create(session, track2.get(), artist2.get(), TrackArtistLinkType::Artist);
	}
	{

		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		CHECK(trackList->getArtistsReverse({}, std::nullopt, std::nullopt, moreResults).empty());
		CHECK(trackList->getReleasesReverse({}, std::nullopt, moreResults).empty());
		CHECK(trackList->getTracksReverse({}, std::nullopt, moreResults).empty());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		TrackListEntry::create(session, track1.get(), trackList.get(), now);
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtistsReverse({}, std::nullopt, std::nullopt, moreResults)};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist1.getId());

		const auto releases {trackList->getReleasesReverse({}, std::nullopt, moreResults)};
		CHECK(releases.size() == 1);
		CHECK(releases.front().id() == release1.getId());

		const auto tracks {trackList->getTracksReverse({}, std::nullopt, moreResults)};
		CHECK(tracks.size() == 1);
	}

	{
		auto transaction {session.createUniqueTransaction()};

		TrackListEntry::create(session, track2.get(), trackList.get(), now.addSecs(1));
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtistsReverse({}, std::nullopt, std::nullopt, moreResults)};
		CHECK(artists.size() == 2);
		CHECK(artists[0].id() == artist2.getId());
		CHECK(artists[1].id() == artist1.getId());

		const auto releases {trackList->getReleasesReverse({}, std::nullopt, moreResults)};
		CHECK(releases.size() == 2);
		CHECK(releases[0].id() == release2.getId());
		CHECK(releases[1].id() == release1.getId());

		const auto tracks {trackList->getTracksReverse({}, std::nullopt, moreResults)};
		CHECK(tracks.size() == 2);
		CHECK(tracks[0].id() == track2.getId());
		CHECK(tracks[1].id() == track1.getId());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		TrackListEntry::create(session, track1.get(), trackList.get(), now.addSecs(2));
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtistsReverse({}, std::nullopt, std::nullopt, moreResults)};
		CHECK(artists.size() == 2);
		CHECK(artists[0].id() == artist1.getId());
		CHECK(artists[1].id() == artist2.getId());

		const auto releases {trackList->getReleasesReverse({}, std::nullopt, moreResults)};
		CHECK(releases.size() == 2);
		CHECK(releases[0].id() == release1.getId());
		CHECK(releases[1].id() == release2.getId());

		const auto tracks {trackList->getTracksReverse({}, std::nullopt, moreResults)};
		CHECK(tracks.size() == 2);
		CHECK(tracks[0].id() == track1.getId());
		CHECK(tracks[1].id() == track2.getId());
	}
}

static
void
testSingleTrackListMultipleTrackMultiClustersRecentlyPlayed(Session& session)
{
	ScopedUser user {session, "MyUser"};
	ScopedTrackList trackList {session, "MyTrackList", TrackList::Type::Playlist, false, user.lockAndGet()};
	ScopedClusterType clusterType {session, "MyClusterType"};
	ScopedCluster cluster1 {session, clusterType.lockAndGet(), "MyCluster1"};
	ScopedCluster cluster2 {session, clusterType.lockAndGet(), "MyCluster2"};
	ScopedCluster cluster3 {session, clusterType.lockAndGet(), "MyCluster3"};
	ScopedTrack track1 {session, "MyTrack1"};
	ScopedTrack track2 {session, "MyTrack1"};
	ScopedArtist artist1 {session, "MyArtist1"};
	ScopedArtist artist2 {session, "MyArtist2"};
	ScopedRelease release1 {session, "MyRelease1"};
	ScopedRelease release2 {session, "MyRelease2"};

	const Wt::WDateTime now {Wt::WDateTime::currentDateTime()};

	{
		auto transaction {session.createUniqueTransaction()};

		track1.get().modify()->setRelease(release1.get());
		track2.get().modify()->setRelease(release2.get());
		TrackArtistLink::create(session, track1.get(), artist1.get(), TrackArtistLinkType::Artist);
		TrackArtistLink::create(session, track2.get(), artist2.get(), TrackArtistLinkType::Artist);

		cluster1.get().modify()->addTrack(track1.get());
		cluster2.get().modify()->addTrack(track2.get());
		cluster3.get().modify()->addTrack(track1.get());
		cluster3.get().modify()->addTrack(track2.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		CHECK(trackList->getArtistsReverse({}, std::nullopt, std::nullopt, moreResults).empty());
		CHECK(trackList->getReleasesReverse({}, std::nullopt, moreResults).empty());
		CHECK(trackList->getTracksReverse({}, std::nullopt, moreResults).empty());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		TrackListEntry::create(session, track1.get(), trackList.get(), now);
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtistsReverse({}, std::nullopt, std::nullopt, moreResults)};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist1.getId());

		const auto releases {trackList->getReleasesReverse({}, std::nullopt, moreResults)};
		CHECK(releases.size() == 1);
		CHECK(releases.front().id() == release1.getId());

		const auto tracks {trackList->getTracksReverse({}, std::nullopt, moreResults)};
		CHECK(tracks.size() == 1);
		CHECK(tracks.front().id() == track1.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtistsReverse({cluster1.getId()}, std::nullopt, std::nullopt, moreResults)};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist1.getId());

		const auto releases {trackList->getReleasesReverse({cluster1.getId()}, std::nullopt, moreResults)};
		CHECK(releases.size() == 1);
		CHECK(releases.front().id() == release1.getId());

		const auto tracks {trackList->getTracksReverse({cluster1.getId()}, std::nullopt, moreResults)};
		CHECK(tracks.size() == 1);
		CHECK(tracks.front().id() == track1.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtistsReverse({cluster3.getId()}, std::nullopt, std::nullopt, moreResults)};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist1.getId());

		const auto releases {trackList->getReleasesReverse({cluster3.getId()}, std::nullopt, moreResults)};
		CHECK(releases.size() == 1);
		CHECK(releases.front().id() == release1.getId());

		const auto tracks {trackList->getTracksReverse({cluster3.getId()}, std::nullopt, moreResults)};
		CHECK(tracks.size() == 1);
		CHECK(tracks.front().id() == track1.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtistsReverse({cluster1.getId()}, TrackArtistLinkType::Artist, std::nullopt, moreResults)};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist1.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtistsReverse({}, TrackArtistLinkType::Artist, std::nullopt, moreResults)};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist1.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		CHECK(trackList->getArtistsReverse({cluster2.getId()}, std::nullopt, std::nullopt, moreResults).empty());
		CHECK(trackList->getReleasesReverse({cluster2.getId()}, std::nullopt, moreResults).empty());
		CHECK(trackList->getTracksReverse({cluster2.getId()}, std::nullopt, moreResults).empty());

		CHECK(trackList->getArtistsReverse({}, TrackArtistLinkType::ReleaseArtist, std::nullopt, moreResults).empty());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		TrackListEntry::create(session, track2.get(), trackList.get(), now.addSecs(1));
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtistsReverse({}, std::nullopt, std::nullopt, moreResults)};
		CHECK(artists.size() == 2);
		CHECK(artists[0].id() == artist2.getId());
		CHECK(artists[1].id() == artist1.getId());

		const auto releases {trackList->getReleasesReverse({}, std::nullopt, moreResults)};
		CHECK(releases.size() == 2);
		CHECK(releases[0].id() == release2.getId());
		CHECK(releases[1].id() == release1.getId());

		const auto tracks {trackList->getTracksReverse({}, std::nullopt, moreResults)};
		CHECK(tracks.size() == 2);
		CHECK(tracks[0].id() == track2.getId());
		CHECK(tracks[1].id() == track1.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtistsReverse({cluster3.getId()}, std::nullopt, std::nullopt, moreResults)};
		CHECK(artists.size() == 2);
		CHECK(artists[0].id() == artist2.getId());
		CHECK(artists[1].id() == artist1.getId());

		const auto releases {trackList->getReleasesReverse({cluster3.getId()}, std::nullopt, moreResults)};
		CHECK(releases.size() == 2);
		CHECK(releases[0].id() == release2.getId());
		CHECK(releases[1].id() == release1.getId());

		const auto tracks {trackList->getTracksReverse({cluster3.getId()}, std::nullopt, moreResults)};
		CHECK(tracks.size() == 2);
		CHECK(tracks[0].id() == track2.getId());
		CHECK(tracks[1].id() == track1.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtistsReverse({cluster1.getId()}, std::nullopt, std::nullopt, moreResults)};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist1.getId());

		const auto releases {trackList->getReleasesReverse({cluster1.getId()}, std::nullopt, moreResults)};
		CHECK(releases.size() == 1);
		CHECK(releases.front().id() == release1.getId());

		const auto tracks {trackList->getTracksReverse({cluster1.getId()}, std::nullopt, moreResults)};
		CHECK(tracks.size() == 1);
		CHECK(tracks.front().id() == track1.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtistsReverse({cluster2.getId()}, std::nullopt, std::nullopt, moreResults)};
		CHECK(artists.size() == 1);
		CHECK(artists.front().id() == artist2.getId());

		const auto releases {trackList->getReleasesReverse({cluster2.getId()}, std::nullopt, moreResults)};
		CHECK(releases.size() == 1);
		CHECK(releases.front().id() == release2.getId());

		const auto tracks {trackList->getTracksReverse({cluster2.getId()}, std::nullopt, moreResults)};
		CHECK(tracks.size() == 1);
		CHECK(tracks.front().id() == track2.getId());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		TrackListEntry::create(session, track1.get(), trackList.get(), now.addSecs(2));
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtistsReverse({}, std::nullopt, std::nullopt, moreResults)};
		CHECK(artists.size() == 2);
		CHECK(artists[0].id() == artist1.getId());
		CHECK(artists[1].id() == artist2.getId());

		const auto releases {trackList->getReleasesReverse({}, std::nullopt, moreResults)};
		CHECK(releases.size() == 2);
		CHECK(releases[0].id() == release1.getId());
		CHECK(releases[1].id() == release2.getId());

		const auto tracks {trackList->getTracksReverse({}, std::nullopt, moreResults)};
		CHECK(tracks.size() == 2);
		CHECK(tracks[0].id() == track1.getId());
		CHECK(tracks[1].id() == track2.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtistsReverse({cluster3.getId()}, std::nullopt, std::nullopt, moreResults)};
		CHECK(artists.size() == 2);
		CHECK(artists[0].id() == artist1.getId());
		CHECK(artists[1].id() == artist2.getId());

		const auto releases {trackList->getReleasesReverse({cluster3.getId()}, std::nullopt, moreResults)};
		CHECK(releases.size() == 2);
		CHECK(releases[0].id() == release1.getId());
		CHECK(releases[1].id() == release2.getId());

		const auto tracks {trackList->getTracksReverse({cluster3.getId()}, std::nullopt, moreResults)};
		CHECK(tracks.size() == 2);
		CHECK(tracks[0].id() == track1.getId());
		CHECK(tracks[1].id() == track2.getId());
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
			TrackArtistLink::create(session, tracks.back().get(), artist1.get(), TrackArtistLinkType::Artist);
		else
		{
			TrackArtistLink::create(session, tracks.back().get(), artist2.get(), TrackArtistLinkType::Artist);
			cluster2.get().modify()->addTrack(tracks.back().get());
		}

		cluster1.get().modify()->addTrack(tracks.back().get());
	}

	tracks.emplace_back(session, "MyTrack" + std::to_string(tracks.size()));
	{
		auto transaction {session.createUniqueTransaction()};
		TrackArtistLink::create(session, tracks.back().get(), artist3.get(), TrackArtistLinkType::Artist);
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
			auto artists {artist1->getSimilarArtists({TrackArtistLinkType::Artist})};
			CHECK(artists.size() == 1);
			CHECK(artists.front().id() == artist2.getId());
		}

		{
			auto artists {artist1->getSimilarArtists({TrackArtistLinkType::ReleaseArtist})};
			CHECK(artists.empty() == 1);
		}

		{
			auto artists {artist1->getSimilarArtists({TrackArtistLinkType::Artist, TrackArtistLinkType::ReleaseArtist})};
			CHECK(artists.size() == 1);
			CHECK(artists.front().id() == artist2.getId());
		}

		{
			auto artists {artist1->getSimilarArtists({TrackArtistLinkType::Composer})};
			CHECK(artists.empty());
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
testSingleTrackSingleUserSingleBookmark(Session& session)
{
	ScopedTrack track {session, "MyTrack"};
	ScopedUser user {session, "MyUser"};
	ScopedTrackBookmark bookmark {session, user.lockAndGet(), track.lockAndGet()};

	{
		auto transaction {session.createUniqueTransaction()};

		bookmark.get().modify()->setComment("MyComment");
		bookmark.get().modify()->setOffset(std::chrono::milliseconds {5});
	}

	{
		auto transaction {session.createSharedTransaction()};

		CHECK(TrackBookmark::getAll(session).size() == 1);

		const auto bookmarks {TrackBookmark::getByUser(session, user.get())};
		CHECK(bookmarks.size() == 1);
		CHECK(bookmarks.back() == bookmark.get());
	}
	{
		auto transaction {session.createSharedTransaction()};

		auto userBookmark {TrackBookmark::getByUser(session, user.get(), track.get())};
		CHECK(userBookmark);
		CHECK(userBookmark == bookmark.get());

		CHECK(userBookmark->getOffset() == std::chrono::milliseconds {5});
		CHECK(userBookmark->getComment() == "MyComment");
	}
}

static
void
testDatabaseEmpty(Session& session)
{
	auto uniqueTransaction {session.createUniqueTransaction()};

	CHECK(Artist::getAll(session, Artist::SortMethod::ByName).empty());
	CHECK(Cluster::getAll(session).empty());
	CHECK(ClusterType::getAll(session).empty());
	CHECK(Release::getAll(session).empty());
	CHECK(Track::getAll(session).empty());
	CHECK(TrackBookmark::getAll(session).empty());
	CHECK(TrackList::getAll(session).empty());
	CHECK(User::getAll(session).empty());
}

int main()
{

	try
	{
		// log to stdout
		Service<Logger> logger {std::make_unique<StreamLogger>(std::cout)};

		const std::filesystem::path tmpFile {std::tmpnam(nullptr)};
		ScopedFileDeleter tmpFileDeleter {tmpFile};

		std::cout << "Database test file: '" << tmpFile.string() << "'" << std::endl;

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

		RUN_TEST(testSingleArtistSearchByName);
		RUN_TEST(testMultiArtistsSortMethod);

		RUN_TEST(testSingleTrackSingleRelease);
		RUN_TEST(testMultiTracksSingleReleaseTotalDiscTrack);
		RUN_TEST(testMultiTracksSingleReleaseFirstTrack);

		RUN_TEST(testSingleTrackSingleCluster);
		RUN_TEST(testMultipleTracksSingleCluster);

		RUN_TEST(testMultipleTracksMultipleClustersTopRelease);

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
		RUN_TEST(testSingleTrackListMultipleTrackDateTime);
		RUN_TEST(testSingleTrackListMultipleTrackSingleCluster);
		RUN_TEST(testSingleTrackListMultipleTrackMultiClusters);
		RUN_TEST(testSingleTrackListMultipleTrackRecentlyPlayed);
		RUN_TEST(testSingleTrackListMultipleTrackMultiClustersRecentlyPlayed);
		RUN_TEST(testMultipleTracksMultipleArtistsMultiClusters);
		RUN_TEST(testMultipleTracksMultipleReleasesMultiClusters);

		RUN_TEST(testSingleTrackSingleUserSingleBookmark);
	}
	catch (std::exception& e)
	{
		std::cerr << "Caught exception: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
