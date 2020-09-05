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

#include "common/LoadingIndicator.hpp"
#include "resource/ImageResource.hpp"
#include "Filters.hpp"
#include "LmsApplication.hpp"
#include "MediaPlayer.hpp"
#include "TrackListHelpers.hpp"
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
		addItem(*menu, Wt::WString::tr("Lms.Explore.starred"), Mode::Starred);
		addItem(*menu, Wt::WString::tr("Lms.Explore.recently-played"), Mode::RecentlyPlayed);
		addItem(*menu, Wt::WString::tr("Lms.Explore.most-played"), Mode::MostPlayed);
		addItem(*menu, Wt::WString::tr("Lms.Explore.recently-added"), Mode::RecentlyAdded);
		addItem(*menu, Wt::WString::tr("Lms.Explore.all"), Mode::All);
	}

	Wt::WText* playBtn = bindNew<Wt::WText>("play-btn", Wt::WString::tr("Lms.Explore.template.play-btn"), Wt::TextFormat::XHTML);
	playBtn->clicked().connect([=]
	{
		tracksAction.emit(PlayQueueAction::Play, getAllTracks());
	});

	Wt::WText* moreBtn = bindNew<Wt::WText>("more-btn", Wt::WString::tr("Lms.Explore.template.more-btn"), Wt::TextFormat::XHTML);
	moreBtn->clicked().connect([=]
	{
		Wt::WPopupMenu* popup {LmsApp->createPopupMenu()};

		popup->addItem(Wt::WString::tr("Lms.Explore.play-shuffled"))
			->triggered().connect(this, [this]
			{
				tracksAction.emit(PlayQueueAction::PlayShuffled, getAllTracks());
			});
		popup->addItem(Wt::WString::tr("Lms.Explore.play-last"))
			->triggered().connect(this, [this]
			{
				tracksAction.emit(PlayQueueAction::PlayLast, getAllTracks());
			});

		popup->popup(moreBtn);
	});

	_tracksContainer = bindNew<Wt::WContainerWidget>("tracks");
	hideLoadingIndicator();

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

void
Tracks::displayLoadingIndicator()
{
	_loadingIndicator = bindWidget<Wt::WTemplate>("loading-indicator", createLoadingIndicator());
	_loadingIndicator->scrollVisibilityChanged().connect([this](bool visible)
	{
		if (!visible)
			return;

		addSome();
	});
}

void
Tracks::hideLoadingIndicator()
{
	_loadingIndicator = nullptr;
	bindEmpty("loading-indicator");
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
			range->limit = std::min(*modeLimit - range->offset, range->limit);
		else
			range = Range {0, *modeLimit};
	}

	switch (_mode)
	{
		case Mode::Random:
			tracks = getRandomTracks(range, moreResults);
			break;

		case Mode::Starred:
			tracks = Track::getStarred(LmsApp->getDbSession(), LmsApp->getUser(), _filters->getClusterIds(), range, moreResults);
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

void
Tracks::addSome()
{
	auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	bool moreResults;
	for (const Track::pointer& track : getTracks(Range {static_cast<std::size_t>(_tracksContainer->count()), batchSize}, moreResults))
	{
		_tracksContainer->addWidget(TrackListHelpers::createEntry(track, tracksAction));
	}

	if (moreResults)
		displayLoadingIndicator();
	else
		hideLoadingIndicator();
}

} // namespace UserInterface

