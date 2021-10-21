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

#include <memory>
#include <unordered_set>

#include <Wt/WSignal.h>
#include <Wt/WTemplate.h>

#include "services/database/Types.hpp"
#include "PlayQueueAction.hpp"

namespace Database
{
	class Artist;
	class Release;
}

namespace UserInterface
{

	class Filters;
	class InfiniteScrollingContainer;

	class Artist : public Wt::WTemplate
	{
		public:
			Artist(Filters* filters);

			PlayQueueActionArtistSignal artistsAction;
			PlayQueueActionTrackSignal tracksAction;

		private:
			void refreshView();
			void refreshReleases(const Database::ObjectPtr<Database::Artist>& artist);
			void refreshNonReleaseTracks(const Database::ObjectPtr<Database::Artist>& artist);
			void refreshSimilarArtists(const std::vector<Database::ArtistId>& similarArtistsId);
			void refreshLinks(const Database::ObjectPtr<Database::Artist>& artist);

			void addSomeNonReleaseTracks();
			static constexpr std::size_t _tracksBatchSize {6};
			static constexpr std::size_t _tracksMaxCount {160};

			Filters* _filters {};
			InfiniteScrollingContainer* _trackContainer {};
			Database::ArtistId			_artistId {};
	};
} // namespace UserInterface

