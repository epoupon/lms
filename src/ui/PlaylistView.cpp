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

#include <Wt/WTemplate>
#include <Wt/WText>

#include "PlaylistView.hpp"

namespace UserInterface {

Playlist::Playlist(Wt::WContainerWidget* parent)
: Wt::WContainerWidget(parent)
{
	auto t = new Wt::WTemplate(Wt::WString::tr("template-playlist"), this);
	t->addFunction("tr", &Wt::WTemplate::Functions::tr);

	auto saveBtn = new Wt::WText(Wt::WString::tr("btn-playlist-save-btn"), Wt::XHTMLText);
	t->bindWidget("save-btn", saveBtn);

	auto loadBtn = new Wt::WText(Wt::WString::tr("btn-playlist-load-btn"), Wt::XHTMLText);
	t->bindWidget("load-btn", loadBtn);

	auto clearBtn = new Wt::WText(Wt::WString::tr("btn-playlist-clear-btn"), Wt::XHTMLText);
	t->bindWidget("clear-btn", clearBtn);

	_entriesContainer = new Wt::WContainerWidget();
	t->bindWidget("entries", _entriesContainer);
}

void
Playlist::addTracks(const std::vector<Database::id_type>& trackIds)
{
	for (auto trackId : trackIds)
		std::cerr << "Adding track " << trackId << std::endl;
}

void
Playlist::playTracks(const std::vector<Database::id_type>& trackIds)
{
	for (auto trackId : trackIds)
		std::cerr << "Playing track " << trackId << std::endl;
}

void
Playlist::refresh()
{

}

} // namespace UserInterface

