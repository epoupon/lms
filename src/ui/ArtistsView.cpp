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
#include <Wt/WText>
#include <Wt/WTemplate>
#include <Wt/WLineEdit>

#include "database/Types.hpp"

#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

#include "LmsApplication.hpp"
#include "ArtistsView.hpp"

namespace UserInterface {

using namespace Database;

Artists::Artists(Filters* filters, Wt::WContainerWidget* parent)
: Wt::WContainerWidget(parent),
  _filters(filters)
{
	auto artists = new Wt::WTemplate(Wt::WString::tr("template-artists"), this);
	artists->addFunction("tr", &Wt::WTemplate::Functions::tr);

	auto search = new Wt::WLineEdit();
	artists->bindWidget("search", search);
	search->setPlaceholderText(Wt::WString::tr("msg-search-placeholder"));
	search->textInput().connect(std::bind([this, search]
	{
		auto keywords = splitString(search->text().toUTF8(), " ");
		refresh(keywords);
	}));

	_artistsContainer = new Wt::WContainerWidget();
	artists->bindWidget("artists", _artistsContainer);

	refresh();

	filters->updated().connect(std::bind([=] {
		refresh();
	}));
}

void
Artists::refresh(std::vector<std::string> searchKeywords)
{
	_artistsContainer->clear();

	auto clusterIds = _filters->getClusterIds();

	Wt::Dbo::Transaction transaction(DboSession());

	bool moreResults;
	auto artists = Artist::getByFilter(DboSession(),
			clusterIds,
			searchKeywords,
			0, 40, moreResults);

	for (auto artist : artists)
	{
		Wt::WTemplate* entry = new Wt::WTemplate(Wt::WString::tr("template-artists-entry"), _artistsContainer);

		entry->bindInt("nb-release", artist->getReleases(clusterIds).size());

		{
			Wt::WAnchor *artistAnchor = new Wt::WAnchor(Wt::WLink(Wt::WLink::InternalPath, "/artist/" + std::to_string(artist.id())));
			Wt::WText *artistText = new Wt::WText(artistAnchor);
			artistText->setText(Wt::WString::fromUTF8(artist->getName(), Wt::PlainText));
			entry->bindWidget("name", artistAnchor);
		}
	}

}

} // namespace UserInterface

