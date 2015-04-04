/*
 * Copyright (C) 2013 Emeric Poupon
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

#ifndef UI_AUDIO_DESKTOP_HPP
#define UI_AUDIO_DESKTOP_HPP

#include <string>

#include <Wt/WPopupMenu>

#include "common/SessionData.hpp"

#include "AudioMediaPlayer.hpp"
#include "TrackView.hpp"
#include "PlayQueue.hpp"

#include "FilterChain.hpp"

#include "audio/Audio.hpp"

namespace UserInterface {
namespace Desktop {

class Audio : public UserInterface::Audio
{

	public:

		Audio(SessionData& sessionData, Wt::WContainerWidget* parent = 0);

		void search(std::string searchText);

	private:

		void playlistSaveFromPlayqueue(std::string name);
		void playlistLoadToPlayqueue(std::string name);
		void playlistShowSaveNewDialog();
		void playlistShowSaveDialog(std::string name);
		void playlistShowDeleteDialog(std::string name);
		void playlistRefreshMenus();

		void playTrack(boost::filesystem::path p, int pos);

		enum PlayQueueAddType
		{
			PlayQueueAddAllTracks,
			PlayQueueAddSelectedTracks,
		};

		void playSelectedTracks(PlayQueueAddType addType);
		void addSelectedTracks();

		void handlePlaylistSelected(Wt::WString name);

		Database::Handler&	_db;

		AudioMediaPlayer*	_mediaPlayer;
		TrackView*		_trackView;
		PlayQueue*		_playQueue;

		FilterChain		_filterChain;

		Wt::WPopupMenu* 	_popupMenuSave;
		Wt::WPopupMenu* 	_popupMenuLoad;
		Wt::WPopupMenu* 	_popupMenuDelete;
};

} // namespace Desktop
} // namespace UserInterface

#endif

