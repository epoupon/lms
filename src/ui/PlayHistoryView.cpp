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

#include "PlayHistoryView.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WText.h>

#include "utils/Logger.hpp"

#include "database/Track.hpp"
#include "database/TrackList.hpp"

#include "LmsApplication.hpp"

namespace {

using namespace UserInterface;

std::unique_ptr<Wt::WTemplate> createEntry(Database::Track::pointer track)
{
	auto entry = std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.PlayHistory.template.entry"));

	entry->bindString("name", Wt::WString::fromUTF8(track->getName()), Wt::TextFormat::Plain);

	auto artist = track->getArtist();
	if (artist)
	{
		entry->setCondition("if-has-artist", true);
		entry->bindWidget("artist-name", LmsApplication::createArtistAnchor(track->getArtist()));
	}
	auto release = track->getRelease();
	if (release)
	{
		entry->setCondition("if-has-release", true);
		entry->bindWidget("release-name", LmsApplication::createReleaseAnchor(track->getRelease()));
	}

	return entry;
}

}

namespace UserInterface {

PlayHistory::PlayHistory()
: Wt::WTemplate(Wt::WString::tr("Lms.PlayHistory.template"))
{
	addFunction("tr", &Wt::WTemplate::Functions::tr);

	_entriesContainer = bindNew<Wt::WContainerWidget>("entries");

	// TODO move "Lms.Explore.show-more"
	_showMore = bindNew<Wt::WTemplate>("show-more", Wt::WString::tr("Lms.Explore.show-more"));
	_showMore->addFunction("tr", &Wt::WTemplate::Functions::tr);
	_showMore->setHidden(true);

	_showMore->clicked().connect([=]
	{
		addSome();
	});

	addSome();
}

void
PlayHistory::addTrack(Database::IdType trackId)
{
	Wt::Dbo::Transaction transaction (LmsApp->getDboSession());

	auto trackEntry = LmsApp->getUser()->getPlayedTrackList().modify()->add(trackId);
	_entriesContainer->insertWidget(0, createEntry(trackEntry->getTrack()));
}


void
PlayHistory::addSome()
{
	Wt::Dbo::Transaction transaction (LmsApp->getDboSession());

	auto trackList = LmsApp->getUser()->getPlayedTrackList();
	auto trackEntries = trackList->getEntriesReverse(_entriesContainer->count(), 50);
	for (auto trackEntry : trackEntries)
		_entriesContainer->addWidget(createEntry(trackEntry->getTrack()));

	_showMore->setHidden(static_cast<std::size_t>(_entriesContainer->count()) >= trackList->getCount());
}

} // namespace UserInterface

