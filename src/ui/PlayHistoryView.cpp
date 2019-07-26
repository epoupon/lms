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
#include "database/User.hpp"

#include "LmsApplication.hpp"

namespace {

using namespace UserInterface;

std::unique_ptr<Wt::WTemplate> createEntry(Database::Track::pointer track)
{
	auto entry = std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.PlayHistory.template.entry"));

	entry->bindString("name", Wt::WString::fromUTF8(track->getName()), Wt::TextFormat::Plain);

	auto artists {track->getArtists()};
	auto release = track->getRelease();

	if (!artists.empty() || release)
		entry->setCondition("if-has-artists-or-release", true);

	if (!artists.empty())
	{
		entry->setCondition("if-has-artists", true);

		Wt::WContainerWidget* artistContainer {entry->bindNew<Wt::WContainerWidget>("artists")};
		for (const auto& artist : artists)
		{
			Wt::WTemplate* a {artistContainer->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.PlayHistory.template.entry-artist"))};
			a->bindWidget("artist", LmsApplication::createArtistAnchor(artist));
		}
	}
	if (release)
	{
		entry->setCondition("if-has-release", true);
		entry->bindWidget("release", LmsApplication::createReleaseAnchor(track->getRelease()));
	}

	return entry;
}

}

namespace UserInterface {

PlayHistory::PlayHistory()
: Wt::WTemplate {Wt::WString::tr("Lms.PlayHistory.template")}
{
	addFunction("tr", &Wt::WTemplate::Functions::tr);

	_entriesContainer = bindNew<Wt::WContainerWidget>("entries");

	_showMore = bindNew<Wt::WPushButton>("show-more", Wt::WString::tr("Lms.Explore.show-more"));
	_showMore->setHidden(true);

	_showMore->clicked().connect([=]
	{
		addSome();
	});

	LmsApp->getEvents().trackLoaded.connect([=](Database::IdType trackId, bool /* play */)
	{
		auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

		Database::Track::pointer track {Database::Track::getById(LmsApp->getDbSession(), trackId)};
		if (track)
		{
			Database::TrackListEntry::create(LmsApp->getDbSession(), track, LmsApp->getUser()->getPlayedTrackList(LmsApp->getDbSession()));
			_entriesContainer->insertWidget(0, createEntry(track));
		}
	});

	addSome();
}

void
PlayHistory::addSome()
{
	auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	const Database::TrackList::pointer trackList {LmsApp->getUser()->getPlayedTrackList(LmsApp->getDbSession())};
	auto trackEntries {trackList->getEntriesReverse(_entriesContainer->count(), 50)};
	for (const auto& trackEntry : trackEntries)
		_entriesContainer->addWidget(createEntry(trackEntry->getTrack()));

	_showMore->setHidden(static_cast<std::size_t>(_entriesContainer->count()) >= trackList->getCount());
}

} // namespace UserInterface

