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

#include <Wt/WApplication>
#include <Wt/WAnchor>
#include <Wt/WImage>
#include <Wt/WText>
#include <Wt/WTemplate>

#include "database/Types.hpp"

#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

#include "LmsApplication.hpp"
#include "Filters.hpp"
#include "ReleasesView.hpp"

namespace UserInterface {

using namespace Database;

Releases::Releases(Filters* filters, Wt::WContainerWidget* parent)
: Wt::WContainerWidget(parent),
	_filters(filters)
{
	auto container = new Wt::WTemplate(Wt::WString::tr("Lms.Explore.Releases.template"), this);
	container->addFunction("tr", &Wt::WTemplate::Functions::tr);

	_search = new Wt::WLineEdit();
	container->bindWidget("search", _search);
	_search->setPlaceholderText(Wt::WString::tr("Lms.Explore.search-placeholder"));
	_search->textInput().connect(this, &Releases::refresh);

	_releasesContainer = new Wt::WContainerWidget();
	container->bindWidget("releases", _releasesContainer);

	_showMore = new Wt::WTemplate(Wt::WString::tr("Lms.Explore.show-more"));
	_showMore->addFunction("tr", &Wt::WTemplate::Functions::tr);
	container->bindWidget("show-more", _showMore);

	_showMore->clicked().connect(std::bind([=]
	{
		addSome();
	}));

	refresh();

	filters->updated().connect(this, &Releases::refresh);
}

void
Releases::refresh()
{
	_releasesContainer->clear();
	addSome();
}

void
Releases::addSome()
{
	auto searchKeywords = splitString(_search->text().toUTF8(), " ");

	auto clusterIds = _filters->getClusterIds();

	Wt::Dbo::Transaction transaction(DboSession());

	bool moreResults;
	auto releases = Release::getByFilter(DboSession(), clusterIds, searchKeywords, _releasesContainer->count(), 20, moreResults);

	for (auto release : releases)
	{
		auto releaseId = release.id();

		Wt::WTemplate* entry = new Wt::WTemplate(Wt::WString::tr("Lms.Explore.Releases.template.entry"), _releasesContainer);
		entry->addFunction("tr", Wt::WTemplate::Functions::tr);

		Wt::WAnchor* coverAnchor = LmsApplication::createReleaseAnchor(releaseId);
		Wt::WImage* cover = new Wt::WImage(coverAnchor);
		cover->setImageLink(LmsApp->getImageResource()->getReleaseUrl(releaseId, 128));
		// Some images may not be square
		cover->setWidth(128);
		entry->bindWidget("cover", coverAnchor);

		{
			Wt::WAnchor* releaseAnchor = LmsApplication::createReleaseAnchor(releaseId);
			Wt::WText* releaseName = new Wt::WText(releaseAnchor);
			releaseName->setText(Wt::WString::fromUTF8(release->getName(), Wt::PlainText));
			entry->bindWidget("release-name", releaseAnchor);
		}


		auto artists = release->getArtists();
		if (artists.size() > 1)
		{
			entry->bindString("artist-name", Wt::WString::tr("Lms.Explore.various-artists"));
		}
		else
		{
			Wt::WAnchor* artistAnchor = LmsApplication::createArtistAnchor(artists.front().id());
			Wt::WText* artist = new Wt::WText(artistAnchor);
			artist->setText(Wt::WString::fromUTF8(artists.front()->getName(), Wt::PlainText));
			entry->bindWidget("artist-name", artistAnchor);
		}

		auto playBtn = new Wt::WText(Wt::WString::tr("Lms.Explore.Releases.play"), Wt::XHTMLText);
		entry->bindWidget("play-btn", playBtn);
		playBtn->clicked().connect(std::bind([=]
		{
			releasePlay.emit(releaseId);
		}));

		auto addBtn = new Wt::WText(Wt::WString::tr("Lms.Explore.Releases.add"), Wt::XHTMLText);
		entry->bindWidget("add-btn", addBtn);
		addBtn->clicked().connect(std::bind([=]
		{
			releaseAdd.emit(releaseId);
		}));
	}

	_showMore->setHidden(!moreResults);
}

} // namespace UserInterface

