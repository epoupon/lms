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

#include "TracksView.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WLineEdit.h>
#include <Wt/WText.h>

#include "database/Artist.hpp"
#include "database/Release.hpp"
#include "database/Track.hpp"

#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

#include "resource/ImageResource.hpp"

#include "LmsApplication.hpp"
#include "Filters.hpp"

using namespace Database;

namespace UserInterface {

Tracks::Tracks(Filters* filters)
: Wt::WTemplate(Wt::WString::tr("Lms.Explore.Tracks.template")),
_filters(filters)
{
	addFunction("tr", &Wt::WTemplate::Functions::tr);

	_search = bindNew<Wt::WLineEdit>("search");
	_search->setPlaceholderText(Wt::WString::tr("Lms.Explore.search-placeholder"));
	_search->textInput().connect(this, &Tracks::refresh);

	Wt::WText* playBtn = bindNew<Wt::WText>("play-btn", Wt::WString::tr("Lms.Explore.template.play-btn"), Wt::TextFormat::XHTML);
	playBtn->clicked().connect(std::bind([=]
	{
		Wt::Dbo::Transaction transaction(LmsApp->getDboSession());
		tracksPlay.emit(getTracks());
	}));

	Wt::WText* addBtn = bindNew<Wt::WText>("add-btn", Wt::WString::tr("Lms.Explore.template.add-btn"), Wt::TextFormat::XHTML);
	addBtn->clicked().connect(std::bind([=]
	{
		Wt::Dbo::Transaction transaction(LmsApp->getDboSession());
		tracksAdd.emit(getTracks());
	}));

	_tracksContainer = bindNew<Wt::WContainerWidget>("tracks");

	_showMore = bindNew<Wt::WPushButton>("show-more", Wt::WString::tr("Lms.Explore.show-more"));
	_showMore->clicked().connect(std::bind([=]
	{
		addSome();
	}));

	refresh();

	filters->updated().connect(this, &Tracks::refresh);
}

std::vector<Database::Track::pointer>
Tracks::getTracks(int offset, int size, bool& moreResults)
{
	auto searchKeywords = splitString(_search->text().toUTF8(), " ");
	auto clusterIds = _filters->getClusterIds();

	Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

	return Track::getByFilter(LmsApp->getDboSession(), clusterIds, searchKeywords, offset, size, moreResults);
}

std::vector<Database::Track::pointer>
Tracks::getTracks()
{
	bool moreResults;
	return getTracks(-1, -1, moreResults);
}

void
Tracks::refresh()
{
	_tracksContainer->clear();
	addSome();
}

void
Tracks::addSome()
{
	Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

	bool moreResults;
	auto tracks = getTracks(_tracksContainer->count(), 20, moreResults);

	for (auto track : tracks)
	{
		auto trackId = track.id();
		Wt::WTemplate* entry = _tracksContainer->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Tracks.template.entry"));

		entry->bindString("name", Wt::WString::fromUTF8(track->getName()), Wt::TextFormat::Plain);

		if (track->getArtist())
		{
			entry->setCondition("if-has-artist", true);
			entry->bindWidget("artist-name", LmsApplication::createArtistAnchor(track->getArtist()));
		}

		if (track->getRelease())
		{
			entry->setCondition("if-has-release", true);
			entry->bindWidget("release-name", LmsApplication::createReleaseAnchor(track->getRelease()));
		}

		Wt::WText* playBtn = entry->bindNew<Wt::WText>("play-btn", Wt::WString::tr("Lms.Explore.template.play-btn"), Wt::TextFormat::XHTML);
		playBtn->clicked().connect(std::bind([=]
		{
			trackPlay.emit(trackId);
		}));

		Wt::WText* addBtn = entry->bindNew<Wt::WText>("add-btn", Wt::WString::tr("Lms.Explore.template.add-btn"), Wt::TextFormat::XHTML);
		addBtn->clicked().connect(std::bind([=]
		{
			trackAdd.emit(trackId);
		}));
	}

	_showMore->setHidden(!moreResults);
}

} // namespace UserInterface

