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

#include "Explore.hpp"

#include <Wt/WStackedWidget.h>
#include <Wt/WTemplate.h>
#include <Wt/WText.h>

#include "utils/Logger.hpp"

#include "LmsApplication.hpp"

#include "ArtistsInfoView.hpp"
#include "ArtistsView.hpp"
#include "ArtistView.hpp"
#include "Filters.hpp"
#include "ReleasesInfoView.hpp"
#include "ReleasesView.hpp"
#include "ReleaseView.hpp"
#include "TracksInfoView.hpp"
#include "TracksView.hpp"

namespace UserInterface {

namespace {

void
handleContentsPathChange(Wt::WStackedWidget* stack)
{
	enum Idx
	{
		IdxArtists = 0,
		IdxArtist,
		IdxReleases,
		IdxRelease,
		IdxTracks,
	};

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

void
handleInfoPathChange(Wt::WStackedWidget* stack)
{
	enum Idx
	{
		IdxArtists = 0,
		IdxReleases,
		IdxTracks,
	};

	static const std::map<std::string, int> indexes =
	{
		{ "/artists",		IdxArtists },
		{ "/artist",		IdxArtists },
		{ "/releases",		IdxReleases },
		{ "/release",		IdxReleases },
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

} // namespace

Explore::Explore()
: Wt::WTemplate(Wt::WString::tr("Lms.Explore.template"))
{
	addFunction("tr", &Functions::tr);

	_filters = bindNew<Filters>("filters");

	// Contents
	Wt::WStackedWidget* contentsStack = bindNew<Wt::WStackedWidget>("contents");

	auto artists = std::make_unique<Artists>(_filters);
	artists->artistAdd.connect(this, &Explore::handleArtistAdd);
	artists->artistPlay.connect(this, &Explore::handleArtistPlay);
	contentsStack->addWidget(std::move(artists));

	auto artist = std::make_unique<Artist>(_filters);
	artist->artistAdd.connect(this, &Explore::handleArtistAdd);
	artist->artistPlay.connect(this, &Explore::handleArtistPlay);
	artist->releaseAdd.connect(this, &Explore::handleReleaseAdd);
	artist->releasePlay.connect(this, &Explore::handleReleasePlay);
	contentsStack->addWidget(std::move(artist));

	auto releases = std::make_unique<Releases>(_filters);
	releases->releaseAdd.connect(this, &Explore::handleReleaseAdd);
	releases->releasePlay.connect(this, &Explore::handleReleasePlay);
	contentsStack->addWidget(std::move(releases));

	auto release = std::make_unique<Release>(_filters);
	release->releaseAdd.connect(this, &Explore::handleReleaseAdd);
	release->releasePlay.connect(this, &Explore::handleReleasePlay);
	release->trackAdd.connect(this, &Explore::handleTrackAdd);
	release->trackPlay.connect(this, &Explore::handleTrackPlay);
	contentsStack->addWidget(std::move(release));

	auto tracks = std::make_unique<Tracks>(_filters);
	tracks->trackAdd.connect(this, &Explore::handleTrackAdd);
	tracks->trackPlay.connect(this, &Explore::handleTrackPlay);
	tracks->tracksAdd.connect(this, &Explore::handleTracksAdd);
	tracks->tracksPlay.connect(this, &Explore::handleTracksPlay);
	contentsStack->addWidget(std::move(tracks));

	// Info
	Wt::WStackedWidget* infoStack = bindNew<Wt::WStackedWidget>("info");

	auto artistsInfo = std::make_unique<ArtistsInfo>();
	auto artistsInfoRaw = artistsInfo.get();
	infoStack->addWidget(std::move(artistsInfo));

	auto releasesInfo = std::make_unique<ReleasesInfo>();
	auto releasesInfoRaw = releasesInfo.get();
	infoStack->addWidget(std::move(releasesInfo));

	auto tracksInfo = std::make_unique<TracksInfo>();
	auto tracksInfoRaw = tracksInfo.get();
	infoStack->addWidget(std::move(tracksInfo));

	_dbChanged.connect([=]
	{
		artistsInfoRaw->refreshRecentlyAdded();
		releasesInfoRaw->refreshRecentlyAdded();
		tracksInfoRaw->refreshRecentlyAdded();
	});

	_trackPlayed.connect([=]
	{
		artistsInfoRaw->refreshMostPlayed();
		releasesInfoRaw->refreshMostPlayed();
		tracksInfoRaw->refreshMostPlayed();
	});


	wApp->internalPathChanged().connect(std::bind([=]
	{
		handleContentsPathChange(contentsStack);
		handleInfoPathChange(infoStack);
	}));

	handleContentsPathChange(contentsStack);
	handleInfoPathChange(infoStack);
}

// TODO SQL this?
static std::vector<Database::Track::pointer> getArtistTracks(Wt::Dbo::Session& session, Database::IdType artistId, std::set<Database::IdType> clusters)
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

static std::vector<Database::Track::pointer> getReleaseTracks(Wt::Dbo::Session& session, Database::IdType releaseId, std::set<Database::IdType> clusters)
{
	std::vector<Database::Track::pointer> res;

	auto release = Database::Release::getById(session, releaseId);
	if (!release)
		return res;

	return release->getTracks(clusters);
}

static std::vector<Database::Track::pointer> getTrack(Wt::Dbo::Session& session, Database::IdType trackId)
{
	std::vector<Database::Track::pointer> res;

	auto track = Database::Track::getById(session, trackId);
	if (track)
		res.push_back(track);

	return res;
}

void
Explore::handleArtistAdd(Database::IdType id)
{
	Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

	tracksAdd.emit(getArtistTracks(LmsApp->getDboSession(), id, _filters->getClusterIds()));
}

void
Explore::handleArtistPlay(Database::IdType id)
{
	Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

	tracksPlay.emit(getArtistTracks(LmsApp->getDboSession(), id, _filters->getClusterIds()));
}

void
Explore::handleReleaseAdd(Database::IdType id)
{
	Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

	tracksAdd.emit(getReleaseTracks(LmsApp->getDboSession(), id, _filters->getClusterIds()));
}

void
Explore::handleReleasePlay(Database::IdType id)
{
	Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

	tracksPlay.emit(getReleaseTracks(LmsApp->getDboSession(), id, _filters->getClusterIds()));
}

void
Explore::handleTrackAdd(Database::IdType id)
{
	Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

	tracksAdd.emit(getTrack(LmsApp->getDboSession(), id));
}

void
Explore::handleTrackPlay(Database::IdType id)
{
	Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

	tracksPlay.emit(getTrack(LmsApp->getDboSession(), id));
}

void
Explore::handleTracksAdd(std::vector<Database::Track::pointer> tracks)
{
	tracksAdd.emit(tracks);
}

void
Explore::handleTracksPlay(std::vector<Database::Track::pointer> tracks)
{
	tracksPlay.emit(tracks);
}

} // namespace UserInterface

