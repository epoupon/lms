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

#ifndef AUDIO_WIDGET_HPP
#define AUDIO_WIDGET_HPP

#include <string>

#include <Wt/WLink>
#include <Wt/WImage>
#include <Wt/WContainerWidget>
#include <Wt/WMemoryResource>

#include "audio/AudioMediaPlayerWidget.hpp"
#include "audio/AudioDatabaseWidget.hpp"

namespace UserInterface {

class AudioWidget : public Wt::WContainerWidget
{

	public:

		AudioWidget(SessionData& sessionData, Wt::WContainerWidget* parent = 0);

		void search(const std::string& searchText);

	private:

		void playTrack(boost::filesystem::path p);

		void handleTrackEnded(void);

		Database::Handler&	_db;

		AudioDatabaseWidget*	_audioDbWidget;

		AudioMediaPlayerWidget*	_mediaPlayer;

		// Image
		Wt::WMemoryResource	*_imgResource;
		Wt::WLink		_imgLink;
		Wt::WImage		*_img;



};

} // namespace UserInterface

#endif

