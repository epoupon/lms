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
#include <Wt/WMenu.h>
#include <Wt/WLineEdit.h>
#include <Wt/WText.h>

#include "database/Artist.hpp"
#include "database/Release.hpp"
#include "database/Track.hpp"
#include "database/TrackList.hpp"

#include "utils/Logger.hpp"
#include "utils/String.hpp"

#include "resource/ImageResource.hpp"

#include "Filters.hpp"
#include "LmsApplication.hpp"
#include "MediaPlayer.hpp"
#include "TrackStringUtils.hpp"

using namespace Database;

namespace UserInterface {

Tracks::Tracks(Filters* filters)
: Wt::WTemplate {Wt::WString::tr("Lms.Explore.Tracks.template")},
_filters {filters}
{
	addFunction("tr", &Wt::WTemplate::Functions::tr);

	{
		auto* menu {bindNew<Wt::WMenu>("mode")};

		auto addItem = [this](Wt::WMenu& menu,const Wt::WString& str, Mode mode)
		{
			auto* item {menu.addItem(str)};
			item->clicked().connect([this, mode] { refreshView(mode); });

			if (mode == defaultMode)
				item->renderSelected(true);
		};

		addItem(*menu, Wt::WString::tr("Lms.Explore.random"), Mode::Random);
		addItem(*menu, Wt::WString::tr("Lms.Explore.recently-played"), Mode::RecentlyPlayed);
		addItem(*menu, Wt::WString::tr("Lms.Explore.most-played"), Mode::MostPlayed);
		addItem(*menu, Wt::WString::tr("Lms.Explore.recently-added"), Mode::RecentlyAdded);
		addItem(*menu, Wt::WString::tr("Lms.Explore.all"), Mode::All);
	}

	Wt::WText* playBtn = bindNew<Wt::WText>("play-btn", Wt::WString::tr("Lms.Explore.template.play-btn"), Wt::TextFormat::XHTML);
	playBtn->clicked().connect([=]
	{
		tracksPlay.emit(getAllTracks());
	});

	Wt::WText* addBtn = bindNew<Wt::WText>("add-btn", Wt::WString::tr("Lms.Explore.template.add-btn"), Wt::TextFormat::XHTML);
	addBtn->clicked().connect([=]
	{
		tracksAdd.emit(getAllTracks());
	});

	_tracksContainer = bindNew<Wt::WContainerWidget>("tracks");

	_showMore = bindNew<Wt::WPushButton>("show-more", Wt::WString::tr("Lms.Explore.show-more"));
	_showMore->clicked().connect([this]
	{
		addSome();
	});

	filters->updated().connect([this]
	{
		refreshView();
	});

	refreshView();
}

void
Tracks::refreshView()
{
	_tracksContainer->clear();
	_randomTracks.clear();
	addSome();
}

void
Tracks::refreshView(Mode mode)
{
	_mode = mode;
	refreshView();
}

std::vector<Database::Track::pointer>
Tracks::getRandomTracks(std::optional<Range> range, bool& moreResults)
{
	std::vector<Track::pointer> tracks;

	if (_randomTracks.empty())
		_randomTracks = Track::getAllIdsRandom(LmsApp->getDbSession(), _filters->getClusterIds(), maxItemsPerMode[Mode::Random]);

	{
		auto itBegin {std::cbegin(_randomTracks) + std::min(range ? range->offset : 0, _randomTracks.size())};
		auto itEnd {std::cbegin(_randomTracks) + std::min(range ? range->offset + range->limit : _randomTracks.size(), _randomTracks.size())};

		for (auto it {itBegin}; it != itEnd; ++it)
		{
			const Track::pointer track {Track::getById(LmsApp->getDbSession(), *it)};
			if (track)
				tracks.push_back(track);
		}

		moreResults = (itEnd != std::cend(_randomTracks));
	}

	return tracks;
}

std::vector<Track::pointer>
Tracks::getTracks(std::optional<Range> range, bool& moreResults)
{
	std::vector<Track::pointer> tracks;

	const std::optional<std::size_t> modeLimit{maxItemsPerMode[_mode]};
	if (modeLimit)
	{
		if (range)
			range->limit = std::min(*modeLimit - range->offset, range->offset + range->limit);
		else
			range = Range {0, *modeLimit};
	}

	switch (_mode)
	{
		case Mode::Random:
			tracks = getRandomTracks(range, moreResults);
			break;

		case Mode::RecentlyPlayed:
			tracks = LmsApp->getUser()->getPlayedTrackList(LmsApp->getDbSession())->getTracksReverse(_filters->getClusterIds(), range, moreResults);
			break;

		case Mode::MostPlayed:
			tracks = LmsApp->getUser()->getPlayedTrackList(LmsApp->getDbSession())->getTopTracks(_filters->getClusterIds(), range, moreResults);
			break;

		case Mode::RecentlyAdded:
			tracks = Track::getLastWritten(LmsApp->getDbSession(), std::nullopt, _filters->getClusterIds(), range, moreResults);
			break;

		case Mode::All:
			tracks = Track::getByFilter(LmsApp->getDbSession(), _filters->getClusterIds(), {}, range, moreResults);
			break;
	}

	if (range && modeLimit && (range->offset + range->limit == *modeLimit))
		moreResults = false;

	return tracks;
}

std::vector<Database::IdType>
Tracks::getAllTracks()
{
	auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	bool moreResults;
	const auto tracks {getTracks(std::nullopt, moreResults)};

	std::vector<Database::IdType> res;
	res.reserve(tracks.size());
	std::transform(std::cbegin(tracks), std::cend(tracks), std::back_inserter(res), [](const Database::Track::pointer& track) { return track.id(); });

	return res;
}

std::unique_ptr<Wt::WTemplate>
Tracks::createEntry(const Track::pointer& track)
{
	auto entry {std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Tracks.template.entry"))};
	auto* entryPtr {entry.get()};

	entry->bindString("name", Wt::WString::fromUTF8(track->getName()), Wt::TextFormat::Plain);

	auto artists {track->getArtists()};
	auto release {track->getRelease()};
	const auto trackId {track.id()};

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
		{
			Wt::WAnchor* anchor = entry->bindWidget("cover", LmsApplication::createReleaseAnchor(release, false));
			auto cover = std::make_unique<Wt::WImage>();
			cover->setImageLink(LmsApp->getImageResource()->getReleaseUrl(release.id(), 96));
			cover->setStyleClass("Lms-cover-small");
			anchor->setImage(std::move(cover));
		}
	}
	else
	{
		auto cover = entry->bindNew<Wt::WImage>("cover");
		cover->setImageLink(LmsApp->getImageResource()->getTrackUrl(trackId, 96));
		cover->setStyleClass("Lms-cover-small");
	}

	entry->bindString("duration", trackDurationToString(track->getDuration()), Wt::TextFormat::Plain);

	Wt::WText* playBtn = entry->bindNew<Wt::WText>("play-btn", Wt::WString::tr("Lms.Explore.template.play-btn"), Wt::TextFormat::XHTML);
	playBtn->clicked().connect([=]
	{
		tracksPlay.emit({trackId});
	});

	Wt::WText* addBtn = entry->bindNew<Wt::WText>("add-btn", Wt::WString::tr("Lms.Explore.template.add-btn"), Wt::TextFormat::XHTML);
	addBtn->clicked().connect([=]
	{
		tracksAdd.emit({trackId});
	});

	LmsApp->getMediaPlayer()->trackLoaded.connect(entryPtr, [=] (Database::IdType loadedTrackId)
	{
		entryPtr->bindString("is-playing", loadedTrackId == trackId ? "Lms-entry-playing" : "");
	});

	if (auto trackIdLoaded {LmsApp->getMediaPlayer()->getTrackLoaded()})
	{
		if (*trackIdLoaded == trackId)
			entry->bindString("is-playing", "Lms-entry-playing");
	}

	return entry;
}

void
Tracks::addSome()
{
	auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	bool moreResults;
	for (const Track::pointer& track : getTracks(Range {static_cast<std::size_t>(_tracksContainer->count()), batchSize}, moreResults))
	{
		_tracksContainer->addWidget(createEntry(track));
	}

	_showMore->setHidden(!moreResults);
}

} // namespace UserInterface

