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
	start();
}


Engine::~Engine()
{
	stop();
}

void
Engine::start()
{
	LMS_LOG(RECOMMENDATION, INFO) << "Starting recommendation engine...";

	assert(!_running);
	_running = true;

	requestReloadInternal(false);

	_ioService.start();

	LMS_LOG(RECOMMENDATION, INFO) << "Started recommendation engine!";
}

void
Engine::stop()
{
	LMS_LOG(RECOMMENDATION, INFO) << "Stopping recommendation engine...";

	assert(_running);
	_running = false;

	cancelPendingClassifiers();

	_ioService.stop();

	LMS_LOG(RECOMMENDATION, INFO) << "Stopped recommendation engine!";
}

void
Engine::requestReload()
{
	requestReloadInternal(true);
}

void
Engine::requestReloadInternal(bool databaseChanged)
{
	LMS_LOG(RECOMMENDATION, DEBUG) << "Reload requested...";

	_ioService.post([=]()
	{
		reload(databaseChanged);
	});
}

std::vector<Database::IdType>
Engine::getSimilarTracksFromTrackList(Database::Session& session, Database::IdType trackListId, std::size_t maxCount)
{
	std::shared_lock lock {_classifiersMutex};

	std::vector<Database::IdType> res;

	for (const auto& classifierName : _classifierPriorities)
	{
		auto itClassifier {_classifiers.find(classifierName)};
		if (itClassifier == std::cend(_classifiers))
			continue;

		res = itClassifier->second->getSimilarTracksFromTrackList(session, trackListId, maxCount);
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

	for (const auto& classifierName : _classifierPriorities)
	{
		auto itClassifier {_classifiers.find(classifierName)};
		if (itClassifier == std::cend(_classifiers))
			continue;

		res = itClassifier->second->getSimilarTracks(dbSession, trackIds, maxCount);
		if (!res.empty())
		{
			LMS_LOG(RECOMMENDATION, DEBUG) << "Got " << res.size() << " similar tracks using classifier '" << classifierName << "'";
			break;
		}
	}

	return res;
}

std::vector<Database::IdType>
Engine::getSimilarReleases(Database::Session& dbSession, Database::IdType releaseId, std::size_t maxCount)
{
	std::shared_lock lock {_classifiersMutex};

	std::vector<Database::IdType> res;

	for (const auto& classifierName : _classifierPriorities)
	{
		auto itClassifier {_classifiers.find(classifierName)};
		if (itClassifier == std::cend(_classifiers))
			continue;

		res = itClassifier->second->getSimilarReleases(dbSession, releaseId, maxCount);
		if (!res.empty())
		{
			LMS_LOG(RECOMMENDATION, DEBUG) << "Got " << res.size() << " similar releases using classifier '" << classifierName << "'";
			break;
		}
	}

	return res;
}

std::vector<Database::IdType>
Engine::getSimilarArtists(Database::Session& dbSession, Database::IdType artistId, std::size_t maxCount)
{
	std::shared_lock lock {_classifiersMutex};

	std::vector<Database::IdType> res;

	for (const auto& classifierName : _classifierPriorities)
	{
		auto itClassifier {_classifiers.find(classifierName)};
		if (itClassifier == std::cend(_classifiers))
			continue;

		res = itClassifier->second->getSimilarArtists(dbSession, artistId, maxCount);
		if (!res.empty())
		{
			LMS_LOG(RECOMMENDATION, DEBUG) << "Got " << res.size() << " similar artists using classifier '" << classifierName << "'";
			return res;
		}
	}

	return res;
}

void
Engine::reload(bool databaseChanged)
{
	using namespace Database;

	LMS_LOG(RECOMMENDATION, INFO) << "Reloading recommendation engines...";

	const ScanSettings::RecommendationEngineType engineType {[&]()
	{
		auto transaction {_dbSession.createSharedTransaction()};

		return ScanSettings::get(_dbSession)->getRecommendationEngineType();
	}()};

	clearClassifiers();

	switch (engineType)
	{
		case ScanSettings::RecommendationEngineType::Features:
		{
			auto clustersClassifier {createClustersClassifier()};
			auto featuresClassifier {createFeaturesClassifier()};

			setClassifierPriorities({featuresClassifier->getName(), clustersClassifier->getName()});

			initAndAddClassifier(std::move(clustersClassifier), databaseChanged); // init first since faster
			initAndAddClassifier(std::move(featuresClassifier), databaseChanged);
			break;
		}

		case ScanSettings::RecommendationEngineType::Clusters:
			auto clustersClassifier {createClustersClassifier()};

			setClassifierPriorities({clustersClassifier->getName()});

			initAndAddClassifier(std::move(clustersClassifier), databaseChanged);
			break;
	}

	LMS_LOG(RECOMMENDATION, INFO) << "Recommendation engines reloaded!";

	_sigReloaded.emit();
}

void
Engine::setClassifierPriorities(std::initializer_list<std::string_view> classifierPriorities)
{
	std::unique_lock<std::shared_mutex> lock {_classifiersMutex};

	_classifierPriorities.clear();
	std::transform(std::cbegin(classifierPriorities), std::cend(classifierPriorities), std::back_inserter(_classifierPriorities), [](std::string_view name) { return std::string {name}; });
}

void
Engine::clearClassifiers()
{
	std::unique_lock<std::shared_mutex> lock {_classifiersMutex};

	_classifiers.clear();
}

void
Engine::initAndAddClassifier(std::unique_ptr<IClassifier> classifier, bool databaseChanged)
{
	PendingClassifierHandler pendingClassifier {*this, *classifier.get()};

	LMS_LOG(RECOMMENDATION, INFO) << "Initializing classifier '" << classifier->getName() << "'...";
	bool res {classifier->init(_dbSession, databaseChanged)};
	LMS_LOG(RECOMMENDATION, INFO) << "Initializing classifier '" << classifier->getName() << "': " << (res ? "SUCCESS" : "FAILURE");

	if (res)
	{
		std::unique_lock<std::shared_mutex> lock {_classifiersMutex};

		_classifiers.emplace(classifier->getName(), std::move(classifier));
	}
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
