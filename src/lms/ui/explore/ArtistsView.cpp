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

#include "ArtistsView.hpp"

#include <Wt/WPushButton.h>

#include "services/database/Artist.hpp"
#include "services/database/Session.hpp"
#include "services/database/TrackArtistLink.hpp"
#include "utils/EnumSet.hpp"
#include "utils/Logger.hpp"

#include "common/ValueStringModel.hpp"
#include "common/InfiniteScrollingContainer.hpp"
#include "ArtistListHelpers.hpp"
#include "Filters.hpp"
#include "LmsApplication.hpp"

using namespace Database;

namespace UserInterface {

using ArtistLinkModel = ValueStringModel<std::optional<TrackArtistLinkType>>;

Artists::Artists(Filters& filters)
: Wt::WTemplate {Wt::WString::tr("Lms.Explore.Artists.template")}
, _artistCollector {filters, _defaultMode, _maxCount}
{
	addFunction("tr", &Wt::WTemplate::Functions::tr);

	auto bindMenuItem {[this](const std::string& var, const Wt::WString& title, ArtistCollector::Mode mode)
	{
		auto *menuItem {bindNew<Wt::WPushButton>(var, title)};
		menuItem->clicked().connect([=]
		{
			refreshView(mode);
			_currentActiveItem->removeStyleClass("active");
			menuItem->addStyleClass("active");
			_currentActiveItem = menuItem;
		});

		if (mode == _defaultMode)
		{
			_currentActiveItem = menuItem;
			_currentActiveItem->addStyleClass("active");
		}
	}};

	bindMenuItem("random", Wt::WString::tr("Lms.Explore.random"), ArtistCollector::Mode::Random);
	bindMenuItem("starred", Wt::WString::tr("Lms.Explore.starred"), ArtistCollector::Mode::Starred);
	bindMenuItem("recently-played", Wt::WString::tr("Lms.Explore.recently-played"), ArtistCollector::Mode::RecentlyPlayed);
	bindMenuItem("most-played", Wt::WString::tr("Lms.Explore.most-played"), ArtistCollector::Mode::MostPlayed);
	bindMenuItem("recently-added", Wt::WString::tr("Lms.Explore.recently-added"), ArtistCollector::Mode::RecentlyAdded);
	bindMenuItem("all", Wt::WString::tr("Lms.Explore.all"), ArtistCollector::Mode::All);

	_linkType = bindNew<Wt::WComboBox>("link-type");
	_linkType->setModel(std::make_shared<ArtistLinkModel>());
	_linkType->changed().connect([this]
	{
		const std::optional<TrackArtistLinkType> linkType {static_cast<ArtistLinkModel*>(_linkType->model().get())->getValue(_linkType->currentIndex())};
		refreshView(linkType);
	});
	refreshArtistLinkTypes();

	LmsApp->getScannerEvents().scanComplete.connect(this, [this](const Scanner::ScanStats& stats)
	{
		if (stats.nbChanges())
			refreshArtistLinkTypes();
	});

	_container = bindNew<InfiniteScrollingContainer>("artists", Wt::WString::tr("Lms.Explore.Artists.template.container"));
	_container->onRequestElements.connect([this]
	{
		addSome();
	});

	filters.updated().connect([this]
	{
		refreshView();
	});

	refreshView(_artistCollector.getMode());
}

void
Artists::refreshView()
{
	_container->clear();
	_artistCollector.reset();
	addSome();
}

void
Artists::refreshView(ArtistCollector::Mode mode)
{
	_artistCollector.setMode(mode);
	refreshView();
}

void
Artists::refreshView(std::optional<TrackArtistLinkType> linkType)
{
	_artistCollector.setArtistLinkType(linkType);
	refreshView();
}

void
Artists::refreshArtistLinkTypes()
{
	std::shared_ptr<ArtistLinkModel> linkTypeModel {std::static_pointer_cast<ArtistLinkModel>(_linkType->model())};

	EnumSet<TrackArtistLinkType> usedLinkTypes;
	{
		auto transaction {LmsApp->getDbSession().createSharedTransaction()};
		usedLinkTypes = TrackArtistLink::findUsedTypes(LmsApp->getDbSession());
	}

	auto addTypeIfUsed {[&](TrackArtistLinkType linkType, std::string_view stringKey)
	{
		if (!usedLinkTypes.contains(linkType))
			return;

		linkTypeModel->add(Wt::WString::trn(std::string {stringKey}, 2), linkType);
	}};

	linkTypeModel->clear();

	// add default one first (none)
	linkTypeModel->add(Wt::WString::tr("Lms.Explore.Artists.linktype-all"), std::nullopt);

	// TODO: sort by translated strins
	addTypeIfUsed(TrackArtistLinkType::Artist, "Lms.Explore.Artists.linktype-artist");
	addTypeIfUsed(TrackArtistLinkType::ReleaseArtist, "Lms.Explore.Artists.linktype-releaseartist");
	addTypeIfUsed(TrackArtistLinkType::Composer, "Lms.Explore.Artists.linktype-composer");
	addTypeIfUsed(TrackArtistLinkType::Conductor, "Lms.Explore.Artists.linktype-conductor");
	addTypeIfUsed(TrackArtistLinkType::Lyricist, "Lms.Explore.Artists.linktype-lyricist");
	addTypeIfUsed(TrackArtistLinkType::Mixer, "Lms.Explore.Artists.linktype-mixer");
	addTypeIfUsed(TrackArtistLinkType::Performer, "Lms.Explore.Artists.linktype-performer");
	addTypeIfUsed(TrackArtistLinkType::Producer, "Lms.Explore.Artists.linktype-producer");
	addTypeIfUsed(TrackArtistLinkType::Remixer, "Lms.Explore.Artists.linktype-remixer");
}

void
Artists::addSome()
{
	const auto artistIds {_artistCollector.get(Range {static_cast<std::size_t>(_container->getCount()), _batchSize})};

	{
		auto transaction {LmsApp->getDbSession().createSharedTransaction()};

		for (const ArtistId artistId : artistIds.results)
		{
			if (const auto artist {Artist::find(LmsApp->getDbSession(), artistId)})
				_container->add(ArtistListHelpers::createEntry(artist));
		}
	}

	_container->setHasMore(artistIds.moreResults);
}

} // namespace UserInterface

