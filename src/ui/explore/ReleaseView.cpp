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

#include "resource/ImageResource.hpp"

#include "LmsApplication.hpp"
#include "Filters.hpp"
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

	filters->updated().connect(std::bind([=] {
		refresh();
	}));
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

	auto t = new Wt::WTemplate(Wt::WString::tr("Lms.Explore.Release.template"), this);
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
			t->bindString("artist-name", Wt::WString::tr("Lms.Explore.various-artists"));
		}
		else
		{
			Wt::WAnchor *artistAnchor = LmsApplication::createArtistAnchor(artists.front().id());
			Wt::WText *artist = new Wt::WText(artistAnchor);
			artist->setText(Wt::WString::fromUTF8(artists.front()->getName(), Wt::PlainText));
			t->bindWidget("artist-name", artistAnchor);
		}
	}

	Wt::WImage *cover = new Wt::WImage();
	cover->setImageLink(Wt::WLink(LmsApp->getImageResource()->getReleaseUrl(release.id(), 512)));
	t->bindWidget("cover", cover);

	auto clusterContainers = new Wt::WContainerWidget();
	t->bindWidget("clusters", clusterContainers);

	{
		auto clusters = release->getClusters(3);

		for (auto cluster : clusters)
		{
			auto entry = new Wt::WTemplate(Wt::WString::tr("Lms.Explore.Release.template.cluster-entry"), clusterContainers);

			entry->bindString("name", Wt::WString::fromUTF8(cluster->getName()), Wt::PlainText);
		}
	}

	{
		auto playBtn = new Wt::WText(Wt::WString::tr("Lms.Explore.Release.play"), Wt::XHTMLText);
		t->bindWidget("play-btn", playBtn);
		playBtn->clicked().connect(std::bind([=]
		{
			releasePlay.emit(releaseId);
		}));
	}

	{
		auto addBtn = new Wt::WText(Wt::WString::tr("Lms.Explore.Release.add"), Wt::XHTMLText);
		t->bindWidget("add-btn", addBtn);
		addBtn->clicked().connect(std::bind([=]
		{
			releaseAdd.emit(releaseId);
		}));
	}

	auto tracksContainer = new Wt::WContainerWidget();
	t->bindWidget("tracks", tracksContainer);

	auto clusterIds = _filters->getClusterIds();
	auto tracks = release->getTracks(clusterIds);

	bool variousArtists = release->hasVariousArtists();

	for (auto track : tracks)
	{
		auto trackId = track.id();

		Wt::WTemplate* entry = new Wt::WTemplate(Wt::WString::tr("Lms.Explore.Release.template.entry"), tracksContainer);

		entry->bindString("name", Wt::WString::fromUTF8(track->getName()), Wt::PlainText);

		if (variousArtists && track->getArtist())
		{
			entry->setCondition("if-has-artist", true);
			Wt::WAnchor *artistAnchor = LmsApplication::createArtistAnchor(track->getArtist().id());
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

		auto playBtn = new Wt::WText(Wt::WString::tr("Lms.Explore.Release.play"), Wt::XHTMLText);
		entry->bindWidget("play-btn", playBtn);
		playBtn->clicked().connect(std::bind([=]
		{
			trackPlay.emit(trackId);
		}));

		auto addBtn = new Wt::WText(Wt::WString::tr("Lms.Explore.Release.add"), Wt::XHTMLText);
		entry->bindWidget("add-btn", addBtn);
		addBtn->clicked().connect(std::bind([=]
		{
			trackAdd.emit(trackId);
		}));
	}
}

} // namespace UserInterface

