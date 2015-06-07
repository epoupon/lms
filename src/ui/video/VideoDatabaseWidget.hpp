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

#ifndef VIDEO_DB_WIDGET_HPP
#define VIDEO_DB_WIDGET_HPP

#include <Wt/WTable>
#include <Wt/WContainerWidget>

namespace UserInterface {

class VideoDatabaseWidget : public Wt::WContainerWidget
{
	public:
		VideoDatabaseWidget(Wt::WContainerWidget *parent = 0);

		// Signals
		Wt::Signal< boost::filesystem::path >&	playVideo() { return _playVideo; }

	private:

		void addHeader(void);
		void addDirectory(const std::string& name, boost::filesystem::path path, size_t depth);
		void addVideo(const std::string& name, const boost::posix_time::time_duration& duration, const boost::filesystem::path& path);

		void updateView(boost::filesystem::path directory, size_t depth);

		Wt::Signal< boost::filesystem::path > _playVideo;

		Wt::WTable*	_table;
};

} // namespace UserInterface

#endif

