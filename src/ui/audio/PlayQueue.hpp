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

#include "database/DatabaseHandler.hpp"
#include "database/AudioTypes.hpp"

namespace UserInterface {

class PlayQueue : public Wt::WTableView
{
	public:
		PlayQueue(Database::Handler& db, Wt::WContainerWidget* parent = 0);

		void addTracks(const std::vector<Database::Track::id_type>& trackIds);

		void clear(void);

		void play(void);		// Play the queue from the beginning
		void playNext(void);		// Play the next track
		void playPrevious(void);	// Play the previous track

		// Signals
		Wt::Signal< boost::filesystem::path >& playTrack() { return _sigTrackPlay; }

		// Slots
		void saveToPlaylist(std::string& playListName);
		void loadFromPlaylist(std::string& playListName);
		void handlePlaybackComplete(void);

	private:

		void readTrack(int rowId);

		Wt::Signal< boost::filesystem::path >	_sigTrackPlay;

		Database::Handler&	_db;

		Wt::WStandardItemModel*	_model;

		int			_playedId;

};


} // namespace UserInterface

#endif
