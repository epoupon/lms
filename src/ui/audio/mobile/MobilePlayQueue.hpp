/*
 * Copyright (C) 2016 Emeric Poupon
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

#include <vector>

#include <Wt/WContainerWidget>
#include <Wt/WSignal>

#include "database/Types.hpp"

namespace UserInterface {
namespace Mobile {

class PlayQueue : public Wt::WContainerWidget
{
	public:
		PlayQueue(Wt::WContainerWidget* parent = 0);

		// Contents
		std::size_t addArtist(Database::Artist::id_type id);
		std::size_t addRelease(Database::Release::id_type id);
		std::size_t addTrack(Database::Track::id_type id);
		void clear(void);

		// Controls
		void play(size_t pos);		// Play the track at a given position
		void playNext(void);
		void playPrevious(void);

		void setShuffle(bool enable);	// true to enable shuffle
		void setLoop(bool enable);	// true to enable loop

		// Signals
		// Emitted when a song has to be played
		Wt::Signal<Database::Track::id_type>& playTrack() { return _sigTrackPlay; }

		// Slots
		// Song has finished
		void handlePlaybackComplete(void);

	private:

		bool _loop = false;
		bool _shuffle = false;
		std::size_t _currentPos = 0;

		Wt::WText* _shuffleBtn = nullptr;
		Wt::WText* _loopBtn = nullptr;

		Wt::Signal<Database::Track::id_type>	_sigTrackPlay;

		std::vector<Database::Track::id_type>	_trackIds;
		Wt::WContainerWidget* _trackContainer;
};

} // namespace Mobile
} // namespace UserInterface
