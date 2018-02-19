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
#include <Wt/WLineEdit>
#include <Wt/WTemplate>
#include <Wt/WText>

#include "database/Types.hpp"

#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

#include "LmsApplication.hpp"
#include "Filters.hpp"
#include "TracksView.hpp"

namespace UserInterface {

using namespace Database;

Tracks::Tracks(Filters* filters, Wt::WContainerWidget* parent)
: Wt::WContainerWidget(parent),
	_filters(filters)
{
	auto tracks = new Wt::WTemplate(Wt::WString::tr("template-tracks"), this);
	tracks->addFunction("tr", &Wt::WTemplate::Functions::tr);

	auto search = new Wt::WLineEdit();
	tracks->bindWidget("search", search);
	search->setPlaceholderText(Wt::WString::tr("msg-search-placeholder"));
	search->textInput().connect(std::bind([this, search]
	{
		auto keywords = splitString(search->text().toUTF8(), " ");
		refresh(keywords);
	}));

	_tracksContainer = new Wt::WContainerWidget();
	tracks->bindWidget("tracks", _tracksContainer);

	refresh();

	filters->updated().connect(std::bind([=] {
		refresh();
	}));
}


void
Tracks::refresh(std::vector<std::string> searchKeywords)
{
	_tracksContainer->clear();

	auto clusterIds = _filters->getClusterIds();

	Wt::Dbo::Transaction transaction(DboSession());

	bool moreResults;
	auto tracks = Track::getByFilter(DboSession(), clusterIds, searchKeywords, 0, 40, moreResults);

	for (auto track : tracks)
	{
		auto trackId = track.id();
		Wt::WTemplate* entry = new Wt::WTemplate(Wt::WString::tr("template-tracks-entry"), _tracksContainer);

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
			// TODO anchor
			Wt::WAnchor *releaseAnchor = new Wt::WAnchor(Wt::WLink(Wt::WLink::InternalPath, "/release/" + std::to_string(track->getRelease().id())));
			Wt::WText *releaseText = new Wt::WText(releaseAnchor);
			releaseText->setText(Wt::WString::fromUTF8(release->getName(), Wt::PlainText));
			entry->bindWidget("release-name", releaseAnchor);
		}

		Wt::WImage *cover = new Wt::WImage();
		cover->setImageLink(SessionImageResource()->getTrackUrl(track.id(), 64));
		// Some images may not be square
		cover->setWidth(64);
		entry->bindWidget("cover", cover);

		auto playBtn = new Wt::WText(Wt::WString::tr("btn-tracks-play-btn"), Wt::XHTMLText);
		entry->bindWidget("play-btn", playBtn);
		playBtn->clicked().connect(std::bind([=]
		{
			trackPlay.emit(trackId);
		}));

		auto addBtn = new Wt::WText(Wt::WString::tr("btn-tracks-add-btn"), Wt::XHTMLText);
		entry->bindWidget("add-btn", addBtn);
		addBtn->clicked().connect(std::bind([=]
		{
			trackAdd.emit(trackId);
		}));
	}

}

} // namespace UserInterface

