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

#include "database/Artist.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "utils/Logger.hpp"

#include "LmsApplication.hpp"

#include "ArtistsView.hpp"
#include "ArtistView.hpp"
#include "Filters.hpp"
#include "ReleasesView.hpp"
#include "ReleaseView.hpp"
#include "SearchView.hpp"
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
		IdxSearch,
		IdxTracks,
	};

	static const std::map<std::string, int> indexes =
	{
		{ "/artists",		IdxArtists },
		{ "/artist",		IdxArtist },
		{ "/releases",		IdxReleases },
		{ "/release",		IdxRelease },
		{ "/search",		IdxSearch },
		{ "/tracks",		IdxTracks },
	};

	for (const auto& index : indexes)
	{
		if (wApp->internalPathMatches(index.first))
		{
			stack->setCurrentIndex(index.second);
			return;
		}
	}
}

} // namespace

Explore::Explore(Filters* filters)
: Wt::WTemplate {Wt::WString::tr("Lms.Explore.template")}
, _filters {filters}
{
	addFunction("tr", &Functions::tr);

	// Contents
	Wt::WStackedWidget* contentsStack = bindNew<Wt::WStackedWidget>("contents");
	contentsStack->setAttributeValue("style", "overflow-x:visible;overflow-y:visible;");

	auto artists = std::make_unique<Artists>(_filters);
	contentsStack->addWidget(std::move(artists));

	auto artist = std::make_unique<Artist>(_filters);
	artist->artistsAction.connect(this, &Explore::handleArtistsAction);
	contentsStack->addWidget(std::move(artist));

	auto releases = std::make_unique<Releases>(_filters);
	releases->releasesAction.connect(this, &Explore::handleReleasesAction);
	contentsStack->addWidget(std::move(releases));

	auto release = std::make_unique<Release>(_filters);
	release->releasesAction.connect(this, &Explore::handleReleasesAction);
	release->tracksAction.connect(this, &Explore::handleTracksAction);
	contentsStack->addWidget(std::move(release));

	auto search = std::make_unique<SearchView>(_filters);
	search->tracksAction.connect(this, &Explore::handleTracksAction);
	_search = search.get();
	contentsStack->addWidget(std::move(search));

	auto tracks = std::make_unique<Tracks>(_filters);
	tracks->tracksAction.connect(this, &Explore::handleTracksAction);
	contentsStack->addWidget(std::move(tracks));

	wApp->internalPathChanged().connect(this, [=]
	{
		handleContentsPathChange(contentsStack);
	});

	handleContentsPathChange(contentsStack);
}

void
Explore::search(const Wt::WString& searchText)
{
	_search->refreshView(searchText);
}

static
std::vector<Database::IdType>
getArtistsTracks(Database::Session& session, const std::vector<Database::IdType>& artistsId, const std::set<Database::IdType>&)
{
	std::vector<Database::IdType> res;

	auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	for (Database::IdType artistId : artistsId)
	{
		Database::Artist::pointer artist {Database::Artist::getById(session, artistId)};
		if (!artist)
			continue;

		// TODO handle clusters here
		const std::vector<Database::Track::pointer> tracks {artist->getTracks()};

		res.reserve(res.size() + tracks.size());
		std::transform(std::cbegin(tracks), std::cend(tracks), std::back_inserter(res), [](const Database::Track::pointer& track) { return track.id(); });
	}

	return res;
}

static
std::vector<Database::IdType>
getReleasesTracks(Database::Session& session, const std::vector<Database::IdType>& releasesId, const std::set<Database::IdType>& clusters)
{
	std::vector<Database::IdType> res;

	auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	for (Database::IdType releaseId : releasesId)
	{
		Database::Release::pointer release {Database::Release::getById(session, releaseId)};
		if (!release)
			continue;

		const std::vector<Database::Track::pointer> tracks {release->getTracks(clusters)};

		res.reserve(res.size() + tracks.size());
		std::transform(std::cbegin(tracks), std::cend(tracks), std::back_inserter(res), [](const Database::Track::pointer& track) { return track.id(); });
	}

	return res;
}

void
Explore::handleArtistsAction(PlayQueueAction action, const std::vector<Database::IdType>& artistsId)
{
	tracksAction.emit(action, getArtistsTracks(LmsApp->getDbSession(), artistsId, _filters->getClusterIds()));
}

void
Explore::handleReleasesAction(PlayQueueAction action, const std::vector<Database::IdType>& releasesId)
{
	tracksAction.emit(action, getReleasesTracks(LmsApp->getDbSession(), releasesId, _filters->getClusterIds()));
}

void
Explore::handleTracksAction(PlayQueueAction action, const std::vector<Database::IdType>& tracksId)
{
	tracksAction.emit(action, tracksId);
}


} // namespace UserInterface

