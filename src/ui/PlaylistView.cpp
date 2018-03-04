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

#include <Wt/WAnchor>
#include <Wt/WImage>
#include <Wt/WText>

#include "utils/Logger.hpp"

#include "LmsApplication.hpp"
#include "PlaylistView.hpp"

namespace UserInterface {

static const std::string currentPlaylistName = "__current__playlist__";

Playlist::Playlist(Wt::WContainerWidget* parent)
: Wt::WContainerWidget(parent)
{
	auto container = new Wt::WTemplate(Wt::WString::tr("template-playlist"), this);
	container->addFunction("tr", &Wt::WTemplate::Functions::tr);

	auto saveBtn = new Wt::WText(Wt::WString::tr("btn-playlist-save-btn"), Wt::XHTMLText);
	container->bindWidget("save-btn", saveBtn);

	auto loadBtn = new Wt::WText(Wt::WString::tr("btn-playlist-load-btn"), Wt::XHTMLText);
	container->bindWidget("load-btn", loadBtn);

	auto clearBtn = new Wt::WText(Wt::WString::tr("btn-playlist-clear-btn"), Wt::XHTMLText);
	container->bindWidget("clear-btn", clearBtn);

	_entriesContainer = new Wt::WContainerWidget();
	container->bindWidget("entries", _entriesContainer);

	_showMore = new Wt::WTemplate(Wt::WString::tr("template-show-more"));
	_showMore->addFunction("tr", &Wt::WTemplate::Functions::tr);
	_showMore->setHidden(true);
	container->bindWidget("show-more", _showMore);

	_showMore->clicked().connect(std::bind([=]
	{
		addSome();
	}));

	refresh();
}

void
Playlist::addTracks(const std::vector<Database::Track::pointer>& tracks)
{
	// Use a "session" playlist in order to store the current playlist
	// so that the user can disconnect and get its playlist back

	LMS_LOG(UI, DEBUG) << "Adding tracks to the current queue";

	auto playlist = Database::Playlist::get(DboSession(), currentPlaylistName, CurrentUser());
	if (!playlist)
	{
		playlist = Database::Playlist::create(DboSession(), currentPlaylistName, false, CurrentUser());
	}

	std::size_t pos = playlist->getCount();
	for (auto track : tracks)
	{
		Database::PlaylistEntry::create(DboSession(), track, playlist, pos++);
	}

	addSome();
}

void
Playlist::playTracks(const std::vector<Database::Track::pointer>& tracks)
{
	LMS_LOG(UI, DEBUG) << "Emptying current queue to play new tracks";

	Wt::Dbo::Transaction transaction(DboSession());

	auto playlist = Database::Playlist::get(DboSession(), currentPlaylistName, CurrentUser());
	if (playlist)
		playlist.modify()->clear();

	_entriesContainer->clear();

	addTracks(tracks);

	// TODO Immediate play
}


void
Playlist::refresh()
{
	_entriesContainer->clear();
	addSome();
}

void
Playlist::addSome()
{
	Wt::Dbo::Transaction transaction (DboSession());

	auto playlist = Database::Playlist::get(DboSession(), currentPlaylistName, CurrentUser());
	if (!playlist)
		return;

	bool moreResults;
	auto tracks = playlist->getTracks(_entriesContainer->count(), 50, moreResults);
	for (auto track : tracks)
	{
		Wt::WTemplate* entry = new Wt::WTemplate(Wt::WString::tr("template-playlist-entry"), _entriesContainer);

		entry->bindString("pos", std::to_string(_entriesContainer->count()));
		entry->bindString("name", Wt::WString::fromUTF8(track->getName()), Wt::PlainText);

		auto artist = track->getArtist();
		if (artist)
		{
			entry->setCondition("if-has-artist", true);
			Wt::WAnchor *artistAnchor = new Wt::WAnchor(Wt::WLink(Wt::WLink::InternalPath, "/artist/" + std::to_string(track->getArtist().id())));
			Wt::WText *artistText = new Wt::WText(artistAnchor);
			artistText->setText(Wt::WString::fromUTF8(artist->getName(), Wt::PlainText));
			entry->bindWidget("artist-name", artistAnchor);
		}
		auto release = track->getRelease();
		if (release)
		{
			entry->setCondition("if-has-release", true);
			Wt::WAnchor *releaseAnchor = new Wt::WAnchor(Wt::WLink(Wt::WLink::InternalPath, "/release/" + std::to_string(track->getRelease().id())));
			Wt::WText *releaseText = new Wt::WText(releaseAnchor);
			releaseText->setText(Wt::WString::fromUTF8(release->getName(), Wt::PlainText));
			entry->bindWidget("release-name", releaseAnchor);
		}

		auto playBtn = new Wt::WText(Wt::WString::tr("btn-playlist-entry-play-btn"), Wt::XHTMLText);
		entry->bindWidget("play-btn", playBtn);
		// TODO

		auto addBtn = new Wt::WText(Wt::WString::tr("btn-playlist-entry-del-btn"), Wt::XHTMLText);
		entry->bindWidget("del-btn", addBtn);
		// TODO
	}

	_showMore->setHidden(!moreResults);
}

} // namespace UserInterface

