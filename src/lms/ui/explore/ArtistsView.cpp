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

#include <optional>

#include <Wt/WAnchor.h>
#include <Wt/WLineEdit.h>
#include <Wt/WLocalDateTime.h>
#include <Wt/WTemplate.h>

#include "common/ValueStringModel.hpp"
#include "database/Artist.hpp"
#include "utils/Logger.hpp"
#include "utils/String.hpp"

#include "LmsApplication.hpp"
#include "Filters.hpp"

using namespace Database;

namespace UserInterface {

using ArtistLinkModel = ValueStringModel<std::optional<TrackArtistLink::Type>>;

Artists::Artists(Filters* filters)
: Wt::WTemplate(Wt::WString::tr("Lms.Explore.Artists.template")),
  _filters(filters)
{
	addFunction("tr", &Wt::WTemplate::Functions::tr);

	_search = bindNew<Wt::WLineEdit>("search");
	_search->setPlaceholderText(Wt::WString::tr("Lms.Explore.search-placeholder"));
	_search->textInput().connect(this, &Artists::refresh);

	_linkType = bindNew<Wt::WComboBox>("link-type");
	{
		auto linkTypeModel {std::make_shared<ArtistLinkModel>()};
		linkTypeModel->add(Wt::WString::tr("Lms.Explore.Artists.linktype-all"), {});
		linkTypeModel->add(Wt::WString::tr("Lms.Explore.Artists.linktype-artist"), TrackArtistLink::Type::Artist);
		linkTypeModel->add(Wt::WString::tr("Lms.Explore.Artists.linktype-releaseartist"), TrackArtistLink::Type::ReleaseArtist);
		_linkType->setModel(linkTypeModel);
	}
	_linkType->changed().connect(this, &Artists::refresh);

	_container = bindNew<Wt::WContainerWidget>("artists");

	_showMore = bindNew<Wt::WPushButton>("show-more", Wt::WString::tr("Lms.Explore.show-more"));
	_showMore->clicked().connect(std::bind([=]
	{
		addSome();
	}));

	refresh();

	filters->updated().connect(this, &Artists::refresh);
}

void
Artists::refresh()
{
	_container->clear();
	addSome();
}

void
Artists::addSome()
{
	const auto searchKeywords {StringUtils::splitString(_search->text().toUTF8(), " ")};

	auto clusterIds = _filters->getClusterIds();
	auto linkModel = static_cast<ArtistLinkModel*>(_linkType->model().get());

	auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	bool moreResults {};
	const std::vector<Artist::pointer> artists {Artist::getByFilter(LmsApp->getDbSession(),
			clusterIds,
			searchKeywords,
			linkModel->getValue(_linkType->currentIndex()),
			Artist::NameSortMethod::BySortName,
			_container->count(), 20, moreResults)};

	for (const auto& artist : artists)
	{
		Wt::WTemplate* entry {_container->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Artists.template.entry"))};

		entry->bindWidget("name", LmsApplication::createArtistAnchor(artist));
	}

	_showMore->setHidden(!moreResults);
}

} // namespace UserInterface

