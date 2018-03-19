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

#include <Wt/WContainerWidget>
#include <Wt/WSignal>
#include <Wt/WTemplate>
#include <Wt/WText>

#include <boost/optional.hpp>

#include "database/Types.hpp"

namespace UserInterface {

class PlayQueue : public Wt::WContainerWidget
{
	public:
		PlayQueue(Wt::WContainerWidget* parent = 0);

		void addTracks(const std::vector<Database::Track::pointer>& tracks);
		void playTracks(const std::vector<Database::Track::pointer>& tracks);

		// play the next track in the queue
		void playNext();

		// play the previous track in the queue
		void playPrevious();

		// Signal emitted when a track is to be played
		Wt::Signal<Database::Track::id_type> playTrack;

	private:
		void addSome();
		void updateInfo();
		void updateCurrentTrack(bool selected);

		void play(std::size_t pos);
		void stop();

		Wt::WContainerWidget* _entriesContainer;
		Wt::WTemplate* _showMore;
		Wt::WText* _nbTracks;
		boost::optional<std::size_t> _trackPos;	// current track position, if set
};

} // namespace UserInterface

