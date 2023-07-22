/*
 * Copyright (C) 2018 Emeric Poupon
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
#include <optional>
#include "services/database/ArtistId.hpp"
#include "services/database/Object.hpp"
#include "services/database/ReleaseId.hpp"
#include "services/database/Types.hpp"
#include "utils/EnumSet.hpp"
#include "common/Template.hpp"

namespace Database
{
	class Artist;
	class Release;
}

namespace UserInterface
{
	class Filters;
	class PlayQueueController;
	class InfiniteScrollingContainer;

	class Artist : public Template
	{
		public:
			Artist(Filters& filters, PlayQueueController& controller);

		private:
			void refreshView();
			void refreshReleases();
			void refreshAppearsOnReleases();
			void refreshNonReleaseTracks();
			void refreshSimilarArtists(const std::vector<Database::ArtistId>& similarArtistsId);
			void refreshLinks(const Database::ObjectPtr<Database::Artist>& artist);

			struct ReleaseContainer;
			void addSomeReleases(ReleaseContainer& releaseContainer);
			bool addSomeNonReleaseTracks();
			static constexpr std::size_t _releasesBatchSize {6};
			static constexpr std::size_t _tracksBatchSize {6};
			static constexpr std::size_t _tracksMaxCount {160};

			Filters&					_filters;
			PlayQueueController&		_playQueueController;

			struct ReleaseType
			{
				std::optional<Database::ReleaseTypePrimary> primaryType;
				EnumSet<Database::ReleaseTypeSecondary> secondaryTypes;

				bool operator<(const ReleaseType& other) const;
			};

			struct ReleaseContainer
			{
				InfiniteScrollingContainer* container {};
				std::vector<Database::ReleaseId> releases;
			};
			std::map<ReleaseType, ReleaseContainer> _releaseContainers;
			ReleaseContainer			_appearsOnReleaseContainer {};
			InfiniteScrollingContainer* _trackContainer {};
			Database::ArtistId			_artistId {};
			bool						_needForceRefresh {};
	};
} // namespace UserInterface

