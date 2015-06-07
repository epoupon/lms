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

#ifndef UI_SETTINGS_MEDIA_DIRECTORIES_HPP
#define UI_SETTINGS_MEDIA_DIRECTORIES_HPP

#include <Wt/WStackedWidget>
#include <Wt/WContainerWidget>
#include <Wt/WTable>
#include <Wt/WSignal>

#include "database/MediaDirectory.hpp"

namespace UserInterface {
namespace Settings {

class MediaDirectories : public Wt::WContainerWidget
{
	public:
		MediaDirectories(Wt::WContainerWidget *parent = 0);

		void refresh();

		Wt::Signal<void>& changed()	{ return _sigChanged; }

	private:

		Wt::Signal<void>	_sigChanged;

		void handleMediaDirectoryFormCompleted(bool changed);

		void handleDelMediaDirectory(boost::filesystem::path p, Database::MediaDirectory::Type type);
		void handleCreateMediaDirectory(void);

		Wt::WStackedWidget*	_stack;
		Wt::WTable*		_table;
};

} // namespace UserInterface
} // namespace Settings

#endif

