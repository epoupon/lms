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
		tracksPlay.emit(getTracks());
	}));

	Wt::WText* addBtn = bindNew<Wt::WText>("add-btn", Wt::WString::tr("Lms.Explore.template.add-btn"), Wt::TextFormat::XHTML);
	addBtn->clicked().connect(std::bind([=]
	{
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

std::vector<Database::IdType>
Tracks::getTracks(boost::optional<std::size_t> offset, boost::optional<std::size_t> size, bool& moreResults)
{
	const auto searchKeywords {splitString(_search->text().toUTF8(), " ")};
	const auto clusterIds {_filters->getClusterIds()};

	auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	const auto tracks {Track::getByFilter(LmsApp->getDbSession(), clusterIds, searchKeywords, offset, size, moreResults)};
	std::vector<Database::IdType> res;
	res.reserve(tracks.size());
	std::transform(std::cbegin(tracks), std::cend(tracks), std::back_inserter(res), [](const Database::Track::pointer& track) { return track.id(); });

	return res;

}

std::vector<Database::IdType>
Tracks::getTracks()
{
	bool moreResults;
	return getTracks({}, {}, moreResults);
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
	bool moreResults;
	const std::vector<Database::IdType> trackIds {getTracks(_tracksContainer->count(), 20, moreResults)};

	for (const Database::IdType trackId : trackIds)
	{
		auto transaction {LmsApp->getDbSession().createSharedTransaction()};

		const Database::Track::pointer track {Database::Track::getById(LmsApp->getDbSession(), trackId)};

		Wt::WTemplate* entry {_tracksContainer->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Tracks.template.entry"))};

		entry->bindString("name", Wt::WString::fromUTF8(track->getName()), Wt::TextFormat::Plain);

		auto artists {track->getArtists()};
		auto release {track->getRelease()};

		if (!artists.empty() || release)
			entry->setCondition("if-has-artists-or-release", true);

		if (!artists.empty())
		{
			entry->setCondition("if-has-artists", true);

			Wt::WContainerWidget* artistContainer {entry->bindNew<Wt::WContainerWidget>("artists")};
			for (const auto& artist : artists)
			{
				Wt::WTemplate* a {artistContainer->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Tracks.template.entry-artist"))};
				a->bindWidget("artist", LmsApplication::createArtistAnchor(artist));
			}
		}

		if (track->getRelease())
		{
			entry->setCondition("if-has-release", true);
			entry->bindWidget("release", LmsApplication::createReleaseAnchor(track->getRelease()));
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

