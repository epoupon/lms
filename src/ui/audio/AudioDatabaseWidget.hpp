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

#ifndef AUDIO_DB_WIDGET_HPP
#define AUDIO_DB_WIDGET_HPP

#include <boost/filesystem/path.hpp>

#include <Wt/WContainerWidget>
#include <Wt/WSignal>

#include "common/SessionData.hpp"

#include "FilterWidget.hpp"

namespace UserInterface {

class AudioDatabaseWidget : public Wt::WContainerWidget
{
	public:
		AudioDatabaseWidget( Database::Handler& db, Wt::WContainerWidget *parent = 0);

		void search(const std::string& text);

		void selectNextTrack(void);				// Will later emit the next selected track

		// Signals
		Wt::Signal< boost::filesystem::path >& trackSelected() { return _trackSelected; }

	private:

		Wt::Signal< boost::filesystem::path > _trackSelected;

		void handleTrackSelected(boost::filesystem::path p);
		void handleFilterUpdated(std::size_t idFilter);

		std::vector<FilterWidget*>	_filters;	// Free Search, Genre, Artist, Release, etc.

		bool _refreshingFilters;

};

} // namespace UserInterface

#endif

