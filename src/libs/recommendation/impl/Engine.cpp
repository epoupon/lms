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
#include "utils/Exception.hpp"
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

	cancelPendingClassifiers();

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
	std::shared_lock lock {_classifiersMutex};

	std::vector<Database::IdType> res;

	for (const auto& [priority, classifier] : _classifiers)
	{
		res = classifier->getSimilarTracksFromTrackList(session, trackListId, maxCount);
		if (!res.empty())
			break;
	}

	return res;
}

std::vector<Database::IdType>
Engine::getSimilarTracks(Database::Session& dbSession, const std::unordered_set<Database::IdType>& trackIds, std::size_t maxCount)
{
	std::shared_lock lock {_classifiersMutex};

	std::vector<Database::IdType> res;

	for (const auto& [priority, classifier] : _classifiers)
	{
		res = classifier->getSimilarTracks(dbSession, trackIds, maxCount);
		if (!res.empty())
			break;
	}

	return res;
}

std::vector<Database::IdType>
Engine::getSimilarReleases(Database::Session& dbSession, Database::IdType releaseId, std::size_t maxCount)
{
	std::shared_lock lock {_classifiersMutex};

	std::vector<Database::IdType> res;

	for (const auto& [priority, classifier] : _classifiers)
	{
		res = classifier->getSimilarReleases(dbSession, releaseId, maxCount);
		if (!res.empty())
			break;
	}

	return res;
}

std::vector<Database::IdType>
Engine::getSimilarArtists(Database::Session& dbSession, Database::IdType artistId, std::size_t maxCount)
{
	std::shared_lock lock {_classifiersMutex};

	std::vector<Database::IdType> res;

	for (const auto& [priority, classifier] : _classifiers)
	{
		res = classifier->getSimilarArtists(dbSession, artistId, maxCount);
		if (!res.empty())
			return res;
	}

	return res;
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

	// TODO RAII this
	auto addClassifier = [&](ClassifierPriority prio, std::unique_ptr<IClassifier> classifier)
	{
		try
		{
			addPendingClassifier(*classifier.get());
			bool res {classifier->init(_dbSession)};
			removePendingClassifier(*classifier.get());

			if (res)
				newClassifiers.emplace(prio, std::move(classifier));

			return res;
		}
		catch (LmsException& e)
		{
			removePendingClassifier(*classifier.get());
			throw;
		}
	};

	switch (engineType)
	{
		case ScanSettings::RecommendationEngineType::Features:
			addClassifier(0, createFeaturesClassifier()); // higher priority
			[[fallthrough]];

		case ScanSettings::RecommendationEngineType::Clusters:
			addClassifier(1, createClustersClassifier()); // lower priority
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
Engine::cancelPendingClassifiers()
{
	std::unique_lock<std::shared_mutex> lock {_classifiersMutex};
	
	for (IClassifier* classifier : _pendingClassifiers)
		classifier->requestCancelInit();
}

void
Engine::addPendingClassifier(IClassifier& classifier)
{
	std::unique_lock<std::shared_mutex> lock {_classifiersMutex};

	_pendingClassifiers.insert(&classifier);
}

void
Engine::removePendingClassifier(IClassifier& classifier)
{
	std::unique_lock<std::shared_mutex> lock {_classifiersMutex};

	_pendingClassifiers.erase(&classifier);
}

} // ns Similarity
