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

#include <unordered_map>
#include <vector>

#include "ClustersEngineCreator.hpp"
#include "FeaturesEngineCreator.hpp"

#include "lmscore/database/Db.hpp"
#include "lmscore/database/Session.hpp"
#include "lmscore/database/ScanSettings.hpp"
#include "utils/Exception.hpp"
#include "utils/Logger.hpp"

namespace Recommendation
{

	static
	std::string_view
	engineTypeToString(EngineType engineType)
	{
		switch (engineType)
		{
			case EngineType::Clusters: return "clusters";
			case EngineType::Features: return "features";
		}

		throw LmsException {"Internal error"};
	}

	std::unique_ptr<IEngine>
	createEngine(Database::Db& db)
	{
		return std::make_unique<Engine>(db);
	}

	Engine::Engine(Database::Db& db)
		: _db {db}
	{
	}

	Engine::TrackContainer
	Engine::getSimilarTracksFromTrackList(Database::TrackListId trackListId, std::size_t maxCount) const
	{
		TrackContainer res;

		std::shared_lock lock {_enginesMutex};
		for (const auto& engineType : _enginePriorities)
		{
			auto itEngine {_engines.find(engineType)};
			if (itEngine == std::cend(_engines))
				continue;

			res = itEngine->second->getSimilarTracksFromTrackList(trackListId, maxCount);
			if (!res.empty())
				break;
		}

		return res;
	}

	Engine::TrackContainer
	Engine::getSimilarTracks(const std::vector<Database::TrackId>& trackIds, std::size_t maxCount) const
	{
		TrackContainer res;

		std::shared_lock lock {_enginesMutex};
		for (EngineType engineType : _enginePriorities)
		{
			auto itEngine {_engines.find(engineType)};
			if (itEngine == std::cend(_engines))
				continue;

			const IEngine& engine {*itEngine->second};
			res = engine.getSimilarTracks(trackIds, maxCount);
			if (!res.empty())
			{
				LMS_LOG(RECOMMENDATION, DEBUG) << "Got " << res.size() << " similar tracks using engine '" << engineTypeToString(engineType) << "'";
				break;
			}
		}

		return res;
	}

	Engine::ReleaseContainer
	Engine::getSimilarReleases(Database::ReleaseId releaseId, std::size_t maxCount) const
	{
		ReleaseContainer res;

		std::shared_lock lock {_enginesMutex};
		for (EngineType engineType : _enginePriorities)
		{
			auto itEngine {_engines.find(engineType)};
			if (itEngine == std::cend(_engines))
				continue;

			const IEngine& engine {*itEngine->second};
			res = engine.getSimilarReleases(releaseId, maxCount);
			if (!res.empty())
			{
				LMS_LOG(RECOMMENDATION, DEBUG) << "Got " << res.size() << " similar releases using engine '" << engineTypeToString(engineType) << "'";
				break;
			}

			LMS_LOG(RECOMMENDATION, DEBUG) << "No result using engine '" << engineTypeToString(engineType) << "'";
		}

		return res;
	}

	Engine::ArtistContainer
	Engine::getSimilarArtists(Database::ArtistId artistId, EnumSet<Database::TrackArtistLinkType> linkTypes, std::size_t maxCount) const
	{
		ArtistContainer res;

		std::shared_lock lock {_enginesMutex};
		for (EngineType engineType : _enginePriorities)
		{
			auto itEngine {_engines.find(engineType)};
			if (itEngine == std::cend(_engines))
				continue;

			const IEngine& engine {*itEngine->second};
			res = engine.getSimilarArtists(artistId, linkTypes, maxCount);
			if (!res.empty())
			{
				LMS_LOG(RECOMMENDATION, DEBUG) << "Got " << res.size() << " similar artists using engine '" << engineTypeToString(engineType) << "'";
				return res;
			}
		}

		return res;
	}

	static
	Database::ScanSettings::RecommendationEngineType
	getRecommendationEngineType(Database::Session& session)
	{
		auto transaction {session.createSharedTransaction()};

		return Database::ScanSettings::get(session)->getRecommendationEngineType();
	}

	void
	Engine::load(bool forceReload, const ProgressCallback& progressCallback)
	{
		using namespace Database;

		LMS_LOG(RECOMMENDATION, INFO) << "Reloading recommendation engines...";

		EngineContainer enginesToLoad;

		{
			std::unique_lock controlLock {_controlMutex};

			{
				std::unique_lock lock {_enginesMutex};
				_engines.clear();
			}

			switch (getRecommendationEngineType(_db.getTLSSession()))
			{
				case ScanSettings::RecommendationEngineType::Clusters:
					_enginePriorities = {EngineType::Clusters};
					enginesToLoad.try_emplace(EngineType::Clusters, createClustersEngine(_db));
					break;

				case ScanSettings::RecommendationEngineType::Features:
					_enginePriorities = {EngineType::Features, EngineType::Clusters};

					// not same order since clusters is faster to load
					enginesToLoad.try_emplace(EngineType::Clusters, createClustersEngine(_db));
					enginesToLoad.try_emplace(EngineType::Features, createFeaturesEngine(_db));
					break;
			}

			assert(_pendingEngines.empty());
			for (auto& [engineType, engine] : enginesToLoad)
				_pendingEngines.push_back(engine.get());
		}

		for (auto& [engineType, engine] : enginesToLoad)
			loadPendingEngine(engineType, std::move(engine), forceReload, progressCallback);

		_pendingEnginesCondvar.notify_all();

		LMS_LOG(RECOMMENDATION, INFO) << "Recommendation engines loaded!";
	}

	void
	Engine::loadPendingEngine(EngineType engineType, std::unique_ptr<IEngine> engine, bool forceReload, const ProgressCallback& progressCallback)
	{
		if (!_loadCancelled)
		{
			LMS_LOG(RECOMMENDATION, INFO) << "Initializing engine '" << engineTypeToString(engineType) << "'...";

			auto progress {[&](const IEngine::Progress& progress)
			{
				progressCallback(progress);
			}};

			engine->load(forceReload, progressCallback ? progress : IEngine::ProgressCallback {});

			{
				std::scoped_lock lock {_controlMutex};
				_pendingEngines.erase(std::find(std::begin(_pendingEngines), std::end(_pendingEngines), engine.get()));
			}
			LMS_LOG(RECOMMENDATION, INFO) << "Initializing engine '" << engineTypeToString(engineType) << "': " << (_loadCancelled ? "aborted" : "complete");
		}

		if (!_loadCancelled)
		{
			std::unique_lock lock {_enginesMutex};
			_engines.emplace(engineType, std::move(engine));
		}
	}

	void
	Engine::cancelLoad()
	{
		LMS_LOG(RECOMMENDATION, DEBUG) << "Cancelling loading...";

		std::unique_lock controlLock {_controlMutex};

		assert(!_loadCancelled);
		_loadCancelled = true;

		LMS_LOG(RECOMMENDATION, DEBUG) << "Still " <<  _pendingEngines.size() << " pending engines!";

		for (IEngine* engine : _pendingEngines)
			engine->requestCancelLoad();

		_pendingEnginesCondvar.wait(controlLock, [this] {return _pendingEngines.empty();});
		_loadCancelled = false;

		LMS_LOG(RECOMMENDATION, DEBUG) << "Cancelling loading DONE";
	}

} // ns Similarity
