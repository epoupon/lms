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

#include <Wt/WStackedWidget>
#include <Wt/WTemplate>
#include <Wt/WText>

#include "utils/Logger.hpp"

#include "LmsApplication.hpp"

#include "Filters.hpp"
#include "ArtistsView.hpp"
#include "ArtistView.hpp"
#include "ReleasesView.hpp"
#include "ReleaseView.hpp"
#include "TracksView.hpp"

#include "Explore.hpp"

namespace UserInterface {


enum IdxRoot
{
	IdxArtists = 0,
	IdxArtist,
	IdxReleases,
	IdxRelease,
	IdxTracks,
};

static void
handlePathChange(Wt::WStackedWidget* stack)
{
	static const std::map<std::string, int> indexes =
	{
		{ "/artists",		IdxArtists },
		{ "/artist",		IdxArtist },
		{ "/releases",		IdxReleases },
		{ "/release",		IdxRelease },
		{ "/tracks",		IdxTracks },
	};

	LMS_LOG(UI, DEBUG) << "Internal path changed to '" << wApp->internalPath() << "'";

	for (auto index : indexes)
	{
		if (wApp->internalPathMatches(index.first))
		{
			stack->setCurrentIndex(index.second);
			return;
		}
	}
}

Explore::Explore(Wt::WContainerWidget* parent)
: Wt::WContainerWidget(parent)
{
	auto container = new Wt::WTemplate(Wt::WString::tr("template-explore"), this);

	_filters = new Filters();
	container->bindWidget("filters", _filters);

	// Contents
	Wt::WStackedWidget* stack = new Wt::WStackedWidget();
	container->bindWidget("contents", stack);

	auto artists = new Artists(_filters);
	stack->addWidget(artists);

	artists->artistAdd.connect(this, &Explore::handleArtistAdd);
	artists->artistPlay.connect(this, &Explore::handleArtistPlay);

	auto artist = new Artist(_filters);
	stack->addWidget(artist);

	artist->artistAdd.connect(this, &Explore::handleArtistAdd);
	artist->artistPlay.connect(this, &Explore::handleArtistPlay);
	artist->releaseAdd.connect(this, &Explore::handleReleaseAdd);
	artist->releasePlay.connect(this, &Explore::handleReleasePlay);

	auto releases = new Releases(_filters);
	stack->addWidget(releases);

	releases->releaseAdd.connect(this, &Explore::handleReleaseAdd);
	releases->releasePlay.connect(this, &Explore::handleReleasePlay);

	auto release = new Release(_filters);
	stack->addWidget(release);

	release->releaseAdd.connect(this, &Explore::handleReleaseAdd);
	release->releasePlay.connect(this, &Explore::handleReleasePlay);
	release->trackAdd.connect(this, &Explore::handleTrackAdd);
	release->trackPlay.connect(this, &Explore::handleTrackPlay);

	auto tracks = new Tracks(_filters);
	stack->addWidget(tracks);

	tracks->trackAdd.connect(this, &Explore::handleTrackAdd);
	tracks->trackPlay.connect(this, &Explore::handleTrackPlay);


	wApp->internalPathChanged().connect(std::bind([=]
	{
		handlePathChange(stack);
	}));

	handlePathChange(stack);
}

// TODO SQL this?
static std::vector<Database::Track::pointer> getArtistTracks(Wt::Dbo::Session& session, Database::id_type artistId, std::set<Database::id_type> clusters)
{
	std::vector<Database::Track::pointer> res;

	auto artist = Database::Artist::getById(session, artistId);
	if (!artist)
		return res;

	auto releases = artist->getReleases(clusters);
	for (auto release : releases)
	{
		auto tracks = release->getTracks(clusters);
		for (auto track : tracks)
		{
			if (track->getArtist() && track->getArtist().id() == artistId)
				res.push_back(track);
		}
	}

	return res;
}

static std::vector<Database::Track::pointer> getReleaseTracks(Wt::Dbo::Session& session, Database::id_type releaseId, std::set<Database::id_type> clusters)
{
	std::vector<Database::Track::pointer> res;

	auto release = Database::Release::getById(session, releaseId);
	if (!release)
		return res;

	return release->getTracks(clusters);
}

static std::vector<Database::Track::pointer> getTrack(Wt::Dbo::Session& session, Database::id_type trackId)
{
	std::vector<Database::Track::pointer> res;

	auto track = Database::Track::getById(session, trackId);
	if (track)
		res.push_back(track);

	return res;
}

void
Explore::handleArtistAdd(Database::id_type id)
{
	Wt::Dbo::Transaction transaction(DboSession());

	tracksAdd.emit(getArtistTracks(DboSession(), id, _filters->getClusterIds()));
}

void
Explore::handleArtistPlay(Database::id_type id)
{
	Wt::Dbo::Transaction transaction(DboSession());

	tracksPlay.emit(getArtistTracks(DboSession(), id, _filters->getClusterIds()));
}

void
Explore::handleReleaseAdd(Database::id_type id)
{
	Wt::Dbo::Transaction transaction(DboSession());

	tracksAdd.emit(getReleaseTracks(DboSession(), id, _filters->getClusterIds()));
}

void
Explore::handleReleasePlay(Database::id_type id)
{
	Wt::Dbo::Transaction transaction(DboSession());

	tracksPlay.emit(getReleaseTracks(DboSession(), id, _filters->getClusterIds()));
}

void
Explore::handleTrackAdd(Database::id_type id)
{
	Wt::Dbo::Transaction transaction(DboSession());

	tracksAdd.emit(getTrack(DboSession(), id));
}

void
Explore::handleTrackPlay(Database::id_type id)
{
	Wt::Dbo::Transaction transaction(DboSession());

	tracksPlay.emit(getTrack(DboSession(), id));
}

} // namespace UserInterface

