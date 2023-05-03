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
#include "services/database/TrackId.hpp"
#include "services/database/TrackListId.hpp"

#include "common/Template.hpp"


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

		void play(const std::vector<Database::TrackId>& trackIds);
		void playNext(const std::vector<Database::TrackId>& trackIds);
		void playShuffled(const std::vector<Database::TrackId>& trackIds);
		void playOrAddLast(const std::vector<Database::TrackId>& trackIds); // play if queue empty, otherwise just add last
		void playAtIndex(const std::vector<Database::TrackId>& trackIds, std::size_t index);

		// play the next track in the queue
		void playNext();

		// play the previous track in the queue
		void playPrevious();

		// Signal emitted when a track is to be load(and optionally played)
		Wt::Signal<Database::TrackId, bool /*play*/, float /* replayGain */> trackSelected;

		// Signal emitted when track is unselected (has to be stopped)
		Wt::Signal<> trackUnselected;

		// Signal emitted when track count changed
		Wt::Signal<std::size_t> trackCountChanged;

		void onPlaybackEnded();

		std::size_t getCapacity() const { return _capacity; }
		std::size_t getCount();

	private:
		void initTrackLists();

		void notifyAddedTracks(std::size_t nbAddedTracks) const;
		Database::ObjectPtr<Database::TrackList> getQueue() const;
		bool isFull() const;

		void clearTracks();
		void enqueueTracks(const std::vector<Database::TrackId>& trackIds);
		std::vector<Database::TrackId> getAndClearNextTracks();
		void addSome();
		void addEntry(const Database::ObjectPtr<Database::TrackListEntry>& entry);
		void enqueueRadioTracksIfNeeded();
		void enqueueRadioTracks();
		void updateInfo();
		void updateCurrentTrack(bool selected);
		bool isRepeatAllSet() const;
		bool isRadioModeSet() const;

		void loadTrack(std::size_t pos, bool play);
		void stop();

		std::optional<float> getReplayGain(std::size_t pos, const Database::ObjectPtr<Database::Track>& track) const;
		void saveAsTrackList();

		void exportToNewTrackList(const Wt::WString& name);
		void exportToTrackList(Database::TrackListId trackList);

		const std::size_t _capacity;
		static inline constexpr std::size_t _batchSize {12};

		bool _mediaPlayerSettingsLoaded {};
		Database::TrackListId _queueId {};
		InfiniteScrollingContainer* _entriesContainer {};
		Wt::WText* _nbTracks {};
		Wt::WText* _duration {};
		Wt::WCheckBox* _repeatBtn {};
		Wt::WCheckBox* _radioBtn {};
		std::optional<std::size_t> _trackPos;	// current track position, if set
		bool _isTrackSelected {};
};

} // namespace UserInterface

