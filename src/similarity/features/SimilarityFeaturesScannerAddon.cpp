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
#include "database/ScanSettings.hpp"
#include "database/Track.hpp"
#include "database/TrackFeatures.hpp"
#include "similarity/features/SimilarityFeaturesCache.hpp"
#include "utils/Config.hpp"
#include "utils/Logger.hpp"

namespace Similarity {

static
bool
hasAtLeastOneTrackWithFeatures(Database::Session& session)
{
	auto transaction {session.createSharedTransaction()};
	return !Database::Track::getAllIdsWithFeatures(session, 1).empty();
}

struct TrackInfo
{
	Database::IdType id;
	std::string mbid;
};

static
std::vector<TrackInfo>
getTracksWithMBIDAndMissingFeatures(Database::Session& dbSession)
{
	std::vector<TrackInfo> res;

	auto transaction {dbSession.createSharedTransaction()};

	auto tracks {Database::Track::getAllWithMBIDAndMissingFeatures(dbSession)};
	for (const Database::Track::pointer& track : tracks)
		res.push_back({track.id(), track->getMBID()});

	return res;
}

FeaturesScannerAddon::FeaturesScannerAddon(Database::Db& db)
: _dbSession {db}
{
	std::optional<Similarity::FeaturesCache> cache {Similarity::FeaturesCache::read()};
	if (cache)
	{
		auto searcher {std::make_shared<Similarity::FeaturesSearcher>(_dbSession, *cache, [&]() { return _stopRequested; })};
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
	auto uniqueTransaction {_dbSession.createUniqueTransaction()};

	auto track {Database::Track::getById(_dbSession, trackId)};
	if (!track)
		return;

	track.modify()->setFeatures({});
}

void
FeaturesScannerAddon::preScanComplete()
{
	{
		auto transaction {_dbSession.createSharedTransaction()};

		if (Database::ScanSettings::get(_dbSession)->getSimilarityEngineType() != Database::ScanSettings::SimilarityEngineType::Features)
		{
			LMS_LOG(DBUPDATER, INFO) << "Do not fetch features since the engine type does not make use of them";
			return;
		}
	}

	LMS_LOG(DBUPDATER, DEBUG) << "Getting tracks with missing Features...";
	const std::vector<TrackInfo> tracksInfo {getTracksWithMBIDAndMissingFeatures(_dbSession)};
	LMS_LOG(DBUPDATER, DEBUG) << "Getting tracks with missing Features DONE (found " << tracksInfo.size() << ")";

	if (!tracksInfo.empty())
		Similarity::FeaturesCache::invalidate();

	for (const TrackInfo& trackInfo : tracksInfo)
	{
		if (_stopRequested)
			return;

		fetchFeatures(trackInfo.id, trackInfo.mbid);
	}

	updateSearcher();
}

void
FeaturesScannerAddon::updateSearcher()
{
	LMS_LOG(SIMILARITY, INFO) << "Updating searcher...";

	if (!hasAtLeastOneTrackWithFeatures(_dbSession))
	{
		LMS_LOG(DBUPDATER, INFO) << "No track found with features!";
		std::atomic_store(&_searcher, std::shared_ptr<FeaturesSearcher>{});
		return;
	}

	Similarity::FeaturesSearcher::TrainSettings trainSettings;
	trainSettings.featureSettingsMap = FeaturesSearcher::getDefaultTrainFeatureSettings();

	auto searcher {std::make_shared<FeaturesSearcher>(_dbSession, trainSettings, [&]() { return _stopRequested; })};
	if (searcher->isValid())
	{
		std::atomic_store(&_searcher, searcher);
		FeaturesCache cache{searcher->toCache()};
		cache.write();

		LMS_LOG(DBUPDATER, INFO) << "New features similarity searcher instanciated";
	}
	else
	{
		LMS_LOG(DBUPDATER, ERROR) << "Cannot set up a valid features similarity searcher!";
		std::atomic_store(&_searcher, std::shared_ptr<FeaturesSearcher>{});
	}
}

bool
FeaturesScannerAddon::fetchFeatures(Database::IdType trackId, const std::string& MBID)
{
	std::map<std::string, double> features;

	LMS_LOG(DBUPDATER, DEBUG) << "Fetching low level features for track '" << MBID << "'";
	const std::string data {AcousticBrainz::extractLowLevelFeatures(MBID)};
	if (data.empty())
	{
		LMS_LOG(DBUPDATER, ERROR) << "Track " << trackId << ", MBID = '" << MBID << "': cannot extract features using AcousticBrainz";
		return false;
	}

	{
		auto uniqueTransaction {_dbSession.createUniqueTransaction()};

		Wt::Dbo::ptr<Database::Track> track {Database::Track::getById(_dbSession, trackId)};
		if (!track)
			return false;

		Database::TrackFeatures::create(_dbSession, track, data);
	}

	return true;
}

} // namespace Similarity

