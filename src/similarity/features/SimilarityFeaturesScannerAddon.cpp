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

#include "SimilarityFeaturesScannerAddon.hpp"

#include "AcousticBrainzUtils.hpp"
#include "database/Track.hpp"
#include "database/TrackFeatures.hpp"
#include "similarity/features/SimilarityFeaturesCache.hpp"
#include "utils/Config.hpp"
#include "utils/Logger.hpp"

namespace Similarity {

namespace {

struct TrackInfo
{
	Database::IdType id;
	std::string mbid;
};

std::vector<TrackInfo>
getTracksWithMBIDAndMissingFeatures(Wt::Dbo::Session& session)
{
	std::vector<TrackInfo> res;

	Wt::Dbo::Transaction transaction(session);

	auto tracks {Database::Track::getAllWithMBIDAndMissingFeatures(session)};
	for (const Database::Track::pointer& track : tracks)
		res.push_back({track.id(), track->getMBID()});

	return res;
}

} // namespace

FeaturesScannerAddon::FeaturesScannerAddon(Wt::Dbo::SqlConnectionPool& connectionPool)
: _db(connectionPool)
{
	boost::optional<Similarity::FeaturesCache> cache {Similarity::FeaturesCache::read()};
	if (cache)
	{
		auto searcher {std::make_shared<Similarity::FeaturesSearcher>(_db.getSession(), *cache)};
		if (searcher->isValid())
			std::atomic_store(&_searcher, searcher);
	}
}

std::shared_ptr<Similarity::FeaturesSearcher>
FeaturesScannerAddon::getSearcher()
{
	return std::atomic_load(&_searcher);
}

void
FeaturesScannerAddon::requestStop()
{
	_stopRequested = true;
}

void
FeaturesScannerAddon::trackUpdated(Database::IdType trackId)
{
	Wt::Dbo::Transaction transaction(_db.getSession());

	auto track = Database::Track::getById(_db.getSession(), trackId);
	if (!track)
		return;

	track.modify()->eraseFeatures();
}

void
FeaturesScannerAddon::preScanComplete()
{
	LMS_LOG(DBUPDATER, DEBUG) << "Getting tracks with missing Features...";
	std::vector<TrackInfo> tracksInfo {getTracksWithMBIDAndMissingFeatures(_db.getSession())};
	LMS_LOG(DBUPDATER, DEBUG) << "Getting tracks with missing Features DONE (found " << tracksInfo.size() << ")";

	for (const TrackInfo& trackInfo : tracksInfo)
		fetchFeatures(trackInfo.id, trackInfo.mbid);

	Similarity::FeaturesCache::invalidate();
	updateSearcher();
}

void
FeaturesScannerAddon::updateSearcher()
{
	Wt::Dbo::Transaction transaction {_db.getSession()};
	auto tracks {Database::Track::getAllWithFeatures(_db.getSession())};
	transaction.commit();

	if (tracks.empty())
	{
		LMS_LOG(DBUPDATER, INFO) << "No track suitable for features similarity clustering";
		std::atomic_store(&_searcher, std::shared_ptr<FeaturesSearcher>{});
		return;
	}

	auto searcher {std::make_shared<Similarity::FeaturesSearcher>(_db.getSession(), _stopRequested)};
	if (searcher->isValid())
	{
		std::atomic_store(&_searcher, searcher);
		FeaturesCache cache{searcher->toCache()};
		cache.write();

		LMS_LOG(DBUPDATER, INFO) << "New features similarity searcher instanciated";
	}
	else
		std::atomic_store(&_searcher, std::shared_ptr<FeaturesSearcher>{});

}

bool
FeaturesScannerAddon::fetchFeatures(Database::IdType trackId, const std::string& MBID)
{
	std::map<std::string, double> features;

	LMS_LOG(DBUPDATER, DEBUG) << "Fetching low level features for track '" << MBID << "'";
	std::string data {AcousticBrainz::extractLowLevelFeatures(MBID)};
	if (data.empty())
	{
		LMS_LOG(DBUPDATER, ERROR) << "Cannot extract features using AcousticBrainz!";
		return false;
	}

	Wt::Dbo::Transaction transaction{_db.getSession()};

	Wt::Dbo::ptr<Database::Track> track {Database::Track::getById(_db.getSession(), trackId)};
	if (!track)
		return false;

	LMS_LOG(DBUPDATER, DEBUG) << "Successfully extracted AcousticBrainz lowlevel features for track '" << track->getPath().string() << "'";

	Database::TrackFeatures::create(_db.getSession(), track, data);

	return true;
}

} // namespace Similarity

