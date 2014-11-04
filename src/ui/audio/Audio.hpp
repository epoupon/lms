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

#ifndef AUDIO_HPP
#define AUDIO_HPP

#include <string>

#include <Wt/WContainerWidget>

#include "common/SessionData.hpp"

#include "AudioMediaPlayer.hpp"

#include "FilterChain.hpp"

namespace UserInterface {

class Audio : public Wt::WContainerWidget
{

	public:

		Audio(SessionData& sessionData, Wt::WContainerWidget* parent = 0);

		void search(const std::string& searchText);

	private:

		void playTrack(boost::filesystem::path p);

		void handlePlaylistSelected(Wt::WString name);

		Database::Handler&	_db;

		AudioMediaPlayer*	_mediaPlayer;

		FilterChain		_filterChain;

};

} // namespace UserInterface

#endif

