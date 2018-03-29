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
#include <Wt/WTemplate>
#include <Wt/WLineEdit>

#include "database/Types.hpp"

#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

#include "LmsApplication.hpp"
#include "Filters.hpp"
#include "ArtistsView.hpp"

namespace UserInterface {

using namespace Database;

Artists::Artists(Filters* filters, Wt::WContainerWidget* parent)
: Wt::WContainerWidget(parent),
  _filters(filters)
{
	auto container = new Wt::WTemplate(Wt::WString::tr("Lms.Explore.Artists.template"), this);
	container->addFunction("tr", &Wt::WTemplate::Functions::tr);

	_search = new Wt::WLineEdit();
	container->bindWidget("search", _search);
	_search->setPlaceholderText(Wt::WString::tr("Lms.Explore.search-placeholder"));
	_search->textInput().connect(this, &Artists::refresh);

	_artistsContainer = new Wt::WContainerWidget();
	container->bindWidget("artists", _artistsContainer);

	_showMore = new Wt::WTemplate(Wt::WString::tr("Lms.Explore.template.show-more"));
	_showMore->addFunction("tr", &Wt::WTemplate::Functions::tr);
	container->bindWidget("show-more", _showMore);

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
	_artistsContainer->clear();
	addSome();
}

void
Artists::addSome()
{
	auto searchKeywords = splitString(_search->text().toUTF8(), " ");

	auto clusterIds = _filters->getClusterIds();

	Wt::Dbo::Transaction transaction(DboSession());

	bool moreResults;
	auto artists = Artist::getByFilter(DboSession(),
			clusterIds,
			searchKeywords,
			_artistsContainer->count(), 20, moreResults);

	for (auto artist : artists)
	{
		auto entry = new Wt::WTemplate(Wt::WString::tr("Lms.Explore.Artists.template.entry"), _artistsContainer);

		entry->bindInt("nb-release", artist->getReleases(clusterIds).size());

		{
			Wt::WAnchor *artistAnchor = LmsApplication::createArtistAnchor(artist);
			entry->bindWidget("name", artistAnchor);
		}
	}

	_showMore->setHidden(!moreResults);
}

} // namespace UserInterface

