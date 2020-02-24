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

#pragma once

#include <map>
#include <shared_mutex>
#include <vector>

#include <Wt/WIOService.h>

#include "database/Session.hpp"
#include "recommendation/IEngine.hpp"
#include "recommendation/IClassifier.hpp"

namespace Recommendation
{
	class Engine : public IEngine
	{
		public:
			Engine(Database::Db& db);
			~Engine();

			Engine(const Engine&) = delete;
			Engine(Engine&&) = delete;
			Engine& operator=(const Engine&) = delete;
			Engine& operator=(Engine&&) = delete;

		private:

			void requestReload() override;
			Wt::Signal<>& reloaded() override { return _sigReloaded; }

			std::vector<Database::IdType> getSimilarTracksFromTrackList(Database::Session& session, Database::IdType tracklistId, std::size_t maxCount) override;
			std::vector<Database::IdType> getSimilarTracks(Database::Session& session, const std::unordered_set<Database::IdType>& tracksId, std::size_t maxCount) override;
			std::vector<Database::IdType> getSimilarReleases(Database::Session& session, Database::IdType releaseId, std::size_t maxCount) override;
			std::vector<Database::IdType> getSimilarArtists(Database::Session& session, Database::IdType artistId, std::size_t maxCount) override;

			
			void start();
			void stop();

			void requestReloadInternal(bool databaseChanged);
			void reload(bool databaseChanged);

			void setClassifierPriorities(std::initializer_list<std::string_view> classifierNames);
			void clearClassifiers();
			void initAndAddClassifier(std::unique_ptr<IClassifier> classifier, bool databaseChanged);

			class PendingClassifierHandler
			{
				public:
					PendingClassifierHandler(Engine& engine, IClassifier& classifier) : _engine {engine}, _classifier {classifier}
					{
						_engine.addPendingClassifier(_classifier);
					}

					~PendingClassifierHandler()
					{
						_engine.removePendingClassifier(_classifier);
					}

				private:
					Engine& _engine;
					IClassifier& _classifier;
			};

			void cancelPendingClassifiers();
			void addPendingClassifier(IClassifier& classifier);
			void removePendingClassifier(IClassifier& classifier);

			bool			_running {};
			Wt::WIOService		_ioService;
			Database::Session	_dbSession;
			Wt::Signal<>		_sigReloaded;

			std::shared_mutex	_classifiersMutex;
			std::map<std::string, std::unique_ptr<IClassifier>> _classifiers;
			std::vector<std::string> _classifierPriorities; // ordered by priority
			std::unordered_set<IClassifier*> _pendingClassifiers;
	};

} // ns Recommendation

