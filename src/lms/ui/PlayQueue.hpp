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

#include <optional>

#include <Wt/WCheckBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WTemplate.h>
#include <Wt/WText.h>

#include "services/database/Object.hpp"
#include "services/database/TrackListId.hpp"
#include "PlayQueueAction.hpp"

#include "common/Template.hpp"

namespace Similarity
{
	class Finder;
}

namespace Database
{
	class Track;
	class TrackList;
	class TrackListEntry;
}

namespace UserInterface {

class InfiniteScrollingContainer;

class PlayQueue : public Template
{
	public:
		PlayQueue();

		void processTracks(PlayQueueAction action, const std::vector<Database::TrackId>& trackIds);

		// play the next track in the queue
		void playNext();

		// play the previous track in the queue
		void playPrevious();

		// Signal emitted when a track is to be load(and optionally played)
		Wt::Signal<Database::TrackId, bool /*play*/, float /* replayGain */> trackSelected;

		// Signal emitted when track is unselected (has to be stopped)
		Wt::Signal<> trackUnselected;

		constexpr std::size_t getCapacity() const { return _capacity; }

	private:
		Database::ObjectPtr<Database::TrackList> getTrackList() const;
		bool isFull() const;

		void clearTracks();
		std::size_t enqueueTracks(const std::vector<Database::TrackId>& trackIds);
		void addSome();
		void addEntry(const Database::ObjectPtr<Database::TrackListEntry>& entry);
		void enqueueRadioTracks();
		void updateInfo();
		void updateCurrentTrack(bool selected);
		bool isRepeatAllSet() const;
		bool isRadioModeSet() const;

		void loadTrack(std::size_t pos, bool play);
		void stop();

		void addRadioTrackFromSimilarity(std::shared_ptr<Similarity::Finder> similarityFinder);
		void addRadioTrackFromClusters();
		std::optional<float> getReplayGain(std::size_t pos, const Database::ObjectPtr<Database::Track>& track) const;

		static inline constexpr std::size_t _capacity {1000};
		static inline constexpr std::size_t _batchSize {12};

		bool _mediaPlayerSettingsLoaded {};
		Database::TrackListId _tracklistId {};
		InfiniteScrollingContainer* _entriesContainer {};
		Wt::WText* _nbTracks {};
		Wt::WCheckBox* _repeatBtn {};
		Wt::WCheckBox* _radioBtn {};
		std::optional<std::size_t> _trackPos;	// current track position, if set
};

} // namespace UserInterface

