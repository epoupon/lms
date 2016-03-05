/*
 * Copyright (C) 2014 Emeric Poupon
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

#ifndef PLAY_QUEUE_HPP
#define PLAY_QUEUE_HPP

#include <Wt/WTableView>
#include <Wt/WStandardItemModel>

#include "database/Types.hpp"

namespace UserInterface {
namespace Desktop {

class PlayQueueItemDelegate;
class TrackSelector;

class PlayQueue : public Wt::WTableView
{
	public:
		PlayQueue(Wt::WContainerWidget* parent = 0);

		void addTracks(const std::vector<Database::Track::id_type>& trackIds);
		void getTracks(std::vector<Database::Track::id_type>& trackIds) const;

		void clear(void);

		void setShuffle(bool enable);	// true to enable shuffle
		void setLoop(bool enable);	// true to enable loop

		// Play functions
		void play(void);		// Play the queue from the beginning
		void play(int rowId);		// Play the queue from the given rowId
		void select(int rowId);		// Select the given rowId
		void playNext(void);		// Play the next track
		void playPrevious(void);	// Play the previous track

		// List manipulations
		void delSelected(void);
		void delAll(void);
		void moveSelectedUp(void);
		void moveSelectedDown(void);

		// Signals
		// Emitted when a song has to be played
		Wt::Signal< Database::Track::id_type, int >& playTrack() { return _sigTrackPlay; }
		// Emitted when the list has changed
		Wt::Signal< void >& tracksUpdated() { return _sigTracksUpdated; }

		// Slots
		void handlePlaybackComplete(void);

	private:

		void layoutSizeChanged (int width, int height);

		void readTrack(int rowId);
		void setPlayingTrackPos(int newRowPos);
		void renumber(int firstId, int lastId);

		Wt::Signal< Database::Track::id_type, int >	_sigTrackPlay;
		Wt::Signal< void >	_sigTracksUpdated;

		Wt::WStandardItemModel*	_model;

		PlayQueueItemDelegate*	_itemDelegate;

		int				_curPlayedTrackPos;
		std::unique_ptr<TrackSelector>	_trackSelector;

};

} // namespace Desktop
} // namespace UserInterface

#endif
