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

#include "ReleasesView.hpp"

#include <algorithm>

#include <Wt/WAnchor.h>
#include <Wt/WImage.h>
#include <Wt/WMenu.h>
#include <Wt/WText.h>

#include "database/Release.hpp"
#include "database/User.hpp"
#include "database/TrackList.hpp"
#include "utils/Logger.hpp"
#include "utils/String.hpp"

#include "resource/ImageResource.hpp"
#include "ReleaseListHelpers.hpp"
#include "Filters.hpp"
#include "LmsApplication.hpp"

using namespace Database;

namespace UserInterface {

Releases::Releases(Filters* filters)
: Wt::WTemplate {Wt::WString::tr("Lms.Explore.Releases.template")},
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

	Wt::WText* playBtn {bindNew<Wt::WText>("play-btn", Wt::WString::tr("Lms.Explore.template.play-btn"), Wt::TextFormat::XHTML)};
	playBtn->clicked().connect([this]
	{
		releasesAction.emit(PlayQueueAction::Play, getAllReleases());
	});
	Wt::WText* addBtn {bindNew<Wt::WText>("add-btn", Wt::WString::tr("Lms.Explore.template.add-btn"), Wt::TextFormat::XHTML)};
	addBtn->clicked().connect([this]
	{
		releasesAction.emit(PlayQueueAction::AddLast, getAllReleases());
	});

	_container = bindNew<Wt::WContainerWidget>("releases");

	_showMore = bindNew<Wt::WPushButton>("show-more", Wt::WString::tr("Lms.Explore.show-more"));
	_showMore->clicked().connect([this]
	{
		addSome();
	});

	refreshView(defaultMode);

	filters->updated().connect([this] { refreshView(); });
}

void
Releases::refreshView()
{
	_container->clear();
	_randomReleases.clear();
	addSome();
}

void
Releases::refreshView(Mode mode)
{
	_mode = mode;
	refreshView();
}

void
Releases::addSome()
{
	bool moreResults {};

	auto transaction {LmsApp->getDbSession().createSharedTransaction()};
	const auto releases {getReleases(Range {static_cast<std::size_t>(_container->count()), batchSize}, moreResults)};

	for (const Release::pointer& release : releases)
	{
		_container->addWidget(ReleaseListHelpers::createEntry(release));
	}

	_showMore->setHidden(!moreResults);
}

std::vector<Database::Release::pointer>
Releases::getRandomReleases(std::optional<Range> range, bool& moreResults)
{
	std::vector<Release::pointer> releases;

	if (_randomReleases.empty())
		_randomReleases = Release::getAllIdsRandom(LmsApp->getDbSession(), _filters->getClusterIds(), maxItemsPerMode[Mode::Random]);

	{
		auto itBegin {std::cbegin(_randomReleases) + std::min(range ? range->offset : 0, _randomReleases.size())};
		auto itEnd {std::cbegin(_randomReleases) + std::min(range ? range->offset + range->limit : _randomReleases.size(), _randomReleases.size())};

		for (auto it {itBegin}; it != itEnd; ++it)
		{
			Release::pointer release {Release::getById(LmsApp->getDbSession(), *it)};
			if (release)
				releases.push_back(release);
		}

		moreResults = (itEnd != std::cend(_randomReleases));
	}

	return releases;
}

std::vector<Database::Release::pointer>
Releases::getReleases(std::optional<Range> range, bool& moreResults)
{
	std::vector<Release::pointer> releases;

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
			releases = getRandomReleases(range, moreResults);
			break;

		case Mode::RecentlyPlayed:
			releases = LmsApp->getUser()->getPlayedTrackList(LmsApp->getDbSession())->getReleasesReverse(_filters->getClusterIds(), range, moreResults);
			break;

		case Mode::MostPlayed:
			releases = LmsApp->getUser()->getPlayedTrackList(LmsApp->getDbSession())->getTopReleases(_filters->getClusterIds(), range, moreResults);
			break;

		case Mode::RecentlyAdded:
			releases = Release::getLastWritten(LmsApp->getDbSession(), std::nullopt, _filters->getClusterIds(), range, moreResults);
			break;

		case Mode::All:
			releases = Release::getByFilter(LmsApp->getDbSession(), _filters->getClusterIds(), {}, range, moreResults);
			break;
	}

	if (range && modeLimit && (range->offset + range->limit == *modeLimit))
		moreResults = false;

	return releases;
}

std::vector<Database::IdType>
Releases::getAllReleases()
{
	auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	bool moreResults;
	const auto releases {getReleases(std::nullopt, moreResults)};

	std::vector<IdType> res;
	res.reserve(releases.size());
	std::transform(std::cbegin(releases), std::cend(releases), std::back_inserter(res), [](const Release::pointer& release) { return release.id(); });

	return res;
}

} // namespace UserInterface

