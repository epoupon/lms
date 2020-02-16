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

#include "Engine.hpp"

#include "recommendation/ClustersClassifierCreator.hpp"
#include "recommendation/FeaturesClassifierCreator.hpp"

#include "database/ScanSettings.hpp"
#include "database/TrackList.hpp"
#include "utils/Logger.hpp"

namespace Recommendation {

std::unique_ptr<IEngine>
createEngine(Database::Db& db)
{
	return std::make_unique<Engine>(db);
}

Engine::Engine(Database::Db& db)
: _dbSession {db}
{
}

void
Engine::start()
{
	assert(!_running);
	_running = true;

	requestReload();

	_ioService.start();
}

void
Engine::stop()
{
	assert(_running);
	_running = false;

	_ioService.stop();
}

void
Engine::requestReload()
{
	LMS_LOG(RECOMMENDATION, DEBUG) << "Reload requested...";

	_ioService.post([&]()
	{
		reload();
	});
}

std::vector<Database::IdType>
Engine::getSimilarTracksFromTrackList(Database::Session& session, Database::IdType trackListId, std::size_t maxCount)
{
	const std::unordered_set<Database::IdType> trackIds {[&]() -> std::unordered_set<Database::IdType>
	{
		auto transaction {session.createSharedTransaction()};

		Database::TrackList::pointer trackList {Database::TrackList::getById(session, trackListId)};
		if (trackList)
		{
			const std::vector<Database::IdType> orderedTrackIds {trackList->getTrackIds()};
			return std::unordered_set<Database::IdType> {std::cbegin(orderedTrackIds), std::cend(orderedTrackIds)};
		}

		return {};
	}()};

	if (trackIds.empty())
		return {};

	std::shared_lock lock {_classifiersMutex};

	for (const auto& [priority, classifier] : _classifiers)
	{
		if (std::any_of(std::cbegin(trackIds), std::cend(trackIds), [&](Database::IdType trackId) { return classifier->isTrackClassified(trackId); } ))
			return classifier->getSimilarTracksFromTrackList(session, trackListId, maxCount);
	}

	return {};
}

std::vector<Database::IdType>
Engine::getSimilarTracks(Database::Session& dbSession, const std::unordered_set<Database::IdType>& trackIds, std::size_t maxCount)
{
	std::shared_lock lock {_classifiersMutex};

	for (const auto& [priority, classifier] : _classifiers)
	{
		if (std::any_of(std::cbegin(trackIds), std::cend(trackIds), [&](Database::IdType trackId) { return classifier->isTrackClassified(trackId); } ))
			return classifier->getSimilarTracks(dbSession, trackIds, maxCount);
	}

	return {};
}

std::vector<Database::IdType>
Engine::getSimilarReleases(Database::Session& dbSession, Database::IdType releaseId, std::size_t maxCount)
{
	std::shared_lock lock {_classifiersMutex};

	for (const auto& [priority, classifier] : _classifiers)
	{
		if (classifier->isReleaseClassified(releaseId))
			return classifier->getSimilarReleases(dbSession, releaseId, maxCount);
	}

	return {};
}

std::vector<Database::IdType>
Engine::getSimilarArtists(Database::Session& dbSession, Database::IdType artistId, std::size_t maxCount)
{
	std::shared_lock lock {_classifiersMutex};

	for (const auto& [priority, classifier] : _classifiers)
	{
		if (classifier->isArtistClassified(artistId))
			return classifier->getSimilarArtists(dbSession, artistId, maxCount);
	}

	return {};
}

void
Engine::reload()
{
	using namespace Database;

	LMS_LOG(RECOMMENDATION, INFO) << "Reloading recommendation engines...";

	const ScanSettings::RecommendationEngineType engineType {[&]()
	{
		auto transaction {_dbSession.createSharedTransaction()};

		return ScanSettings::get(_dbSession)->getRecommendationEngineType();
	}()};

	std::map<ClassifierPriority, std::unique_ptr<IClassifier>> newClassifiers;

	switch (engineType)
	{
		case ScanSettings::RecommendationEngineType::Features:
//			newClassifiers.emplace_back(0, createFeaturesClassifier()); // higher priority
//			[[fallthrough]];

		case ScanSettings::RecommendationEngineType::Clusters:
			newClassifiers.emplace(1, createClustersClassifier(_dbSession)); // lower priority
			break;
	}

	{
		std::unique_lock lock {_classifiersMutex};
		_classifiers.swap(newClassifiers);
	}

	LMS_LOG(RECOMMENDATION, INFO) << "Recommendation engines reloaded!";

	_sigReloaded.emit();
}


void
Engine::clearClassifiers()
{
}

void
Engine::addClassifier(std::unique_ptr<IClassifier> classifier, unsigned priority)
{
	std::unique_lock lock {_classifiersMutex};

	_classifiers.emplace(priority, std::move(classifier));
}


} // ns Similarity
