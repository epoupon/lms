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

#include <Wt/WContainerWidget.h>
#include <Wt/WPushButton.h>
#include <Wt/WTemplate.h>
#include <Wt/WText.h>

#include "database/Types.hpp"
#include "PlayQueueAction.hpp"

namespace Similarity {
	class Finder;
}

namespace Database {
	class Track;
	class TrackList;
}

namespace UserInterface {

class PlayQueue : public Wt::WTemplate
{
	public:
		PlayQueue();

		void processTracks(PlayQueueAction action, const std::vector<Database::IdType>& trackIds);

		// play the next track in the queue
		void playNext();

		// play the previous track in the queue
		void playPrevious();

		// Signal emitted when a track is to be load(and optionally played)
		Wt::Signal<Database::IdType /*trackId*/, bool /*play*/, float /* replayGain */> trackSelected;

		// Signal emitted when track is unselected (has to be stopped)
		Wt::Signal<> trackUnselected;

	private:
		Wt::Dbo::ptr<Database::TrackList> getTrackList() const;

		void clearTracks();
		void enqueueTracks(const std::vector<Database::IdType>& trackIds);
		void enqueueTrack(Database::IdType trackId);
		void addSome();
		void enqueueRadioTrack();
		void updateInfo();
		void updateCurrentTrack(bool selected);
		void updateRepeatBtn();
		void updateRadioBtn();

		void loadTrack(std::size_t pos, bool play);
		void stop();

		void addRadioTrackFromSimilarity(std::shared_ptr<Similarity::Finder> similarityFinder);
		void addRadioTrackFromClusters();
		std::optional<float> getReplayGain(std::size_t pos, const Wt::Dbo::ptr<Database::Track>& track) const;

		bool _repeatAll {};
		bool _radioMode {};
		bool _mediaPlayerSettingsLoaded {};
		Database::IdType _tracklistId {};
		Wt::WContainerWidget* _entriesContainer {};
		Wt::WPushButton* _showMore {};
		Wt::WText* _nbTracks {};
		Wt::WText* _repeatBtn {};
		Wt::WText* _radioBtn {};
		std::optional<std::size_t> _trackPos;	// current track position, if set
};

} // namespace UserInterface

