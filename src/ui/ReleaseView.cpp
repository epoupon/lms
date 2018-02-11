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
#include <Wt/WTemplate>
#include <Wt/WText>

#include "database/Types.hpp"

#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

#include "LmsApplication.hpp"
#include "ReleaseView.hpp"

namespace UserInterface {

Release::Release(Filters* filters, Wt::WContainerWidget* parent)
: Wt::WContainerWidget(parent),
	_filters(filters)
{
	wApp->internalPathChanged().connect(std::bind([=]
	{
		refresh();
	}));

	refresh();
}

void
Release::refresh()
{
	if (!wApp->internalPathMatches("/release/"))
		return;

	clear();

	Database::Release::id_type releaseId;

	if (!readAs(wApp->internalPathNextPart("/release/"), releaseId))
		return;

        Wt::Dbo::Transaction transaction(DboSession());

	auto release = Database::Release::getById(DboSession(), releaseId);
	if (!release)
	{
		LmsApp->goHome();
		return;
	}

	auto t = new Wt::WTemplate(Wt::WString::tr("template-release"), this);
	t->addFunction("tr", &Wt::WTemplate::Functions::tr);

	t->bindString("name", Wt::WString::fromUTF8(release->getName()), Wt::PlainText);

	boost::optional<int> year = release->getReleaseYear();
	if (year)
	{
		t->setCondition("if-has-year", true);
		t->bindInt("year", *year);

		boost::optional<int> originalYear = release->getReleaseYear(true);
		if (originalYear && *originalYear != *year)
		{
			t->setCondition("if-has-orig-year", true);
			t->bindInt("orig-year", *originalYear);
		}
	}

	{
		auto artists = release->getArtists();
		if (artists.size() > 1)
		{
			t->bindString("artist-name", Wt::WString::tr("msg-various-artists"));
		}
		else
		{
			Wt::WAnchor *artistAnchor = new Wt::WAnchor(Wt::WLink(Wt::WLink::InternalPath, "/artist/" + std::to_string(artists.front().id())));
			Wt::WText *artist = new Wt::WText(artistAnchor);
			artist->setText(Wt::WString::fromUTF8(artists.front()->getName(), Wt::PlainText));
			t->bindWidget("artist-name", artistAnchor);
		}
	}

	Wt::WImage *cover = new Wt::WImage();
	cover->setImageLink(Wt::WLink (SessionImageResource()->getReleaseUrl(release.id(), 512)));
	t->bindWidget("cover", cover);

	{
		auto playBtn = new Wt::WText(Wt::WString::tr("btn-release-play-btn"), Wt::XHTMLText);
		t->bindWidget("play-btn", playBtn);
	}

	{
		auto addBtn = new Wt::WText(Wt::WString::tr("btn-release-add-btn"), Wt::XHTMLText);
		t->bindWidget("add-btn", addBtn);
	}

	auto tracksContainer = new Wt::WContainerWidget();
	t->bindWidget("tracks", tracksContainer);

	auto clusterIds = _filters->getClusterIds();
	auto tracks = release->getTracks(clusterIds);

	bool variousArtists = release->hasVariousArtists();

	for (auto track : tracks)
	{
		Wt::WTemplate* entry = new Wt::WTemplate(Wt::WString::tr("template-release-entry"), tracksContainer);

		entry->bindString("name", Wt::WString::fromUTF8(track->getName()), Wt::PlainText);

		if (variousArtists && track->getArtist())
		{
			entry->setCondition("if-has-artist", true);
			Wt::WAnchor *artistAnchor = new Wt::WAnchor(Wt::WLink(Wt::WLink::InternalPath, "/artist/" + std::to_string(track->getArtist().id())));
			Wt::WText *artistText = new Wt::WText(artistAnchor);
			artistText->setText(Wt::WString::fromUTF8(track->getArtist()->getName(), Wt::PlainText));
			entry->bindWidget("artist-name", artistAnchor);
		}

		auto trackNumber = track->getTrackNumber();
		if (trackNumber)
		{
			entry->setCondition("if-has-track-number", true);
			entry->bindInt("track-number", *trackNumber);
		}

		auto discNumber = track->getDiscNumber();
		auto totalDiscNumber = track->getTotalDiscNumber();
		if (discNumber && totalDiscNumber && *totalDiscNumber > 1)
		{
			entry->setCondition("if-has-disc-number", true);
			entry->bindInt("disc-number", *discNumber);
		}

		auto playBtn = new Wt::WText(Wt::WString::tr("btn-release-play-btn"), Wt::XHTMLText);
		entry->bindWidget("play-btn", playBtn);

		auto addBtn = new Wt::WText(Wt::WString::tr("btn-release-add-btn"), Wt::XHTMLText);
		entry->bindWidget("add-btn", addBtn);
	}
}

} // namespace UserInterface

