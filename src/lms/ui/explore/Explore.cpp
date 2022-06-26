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

#include "services/database/Artist.hpp"
#include "services/database/Release.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "utils/Logger.hpp"

#include "LmsApplication.hpp"

#include "ArtistsView.hpp"
#include "ArtistView.hpp"
#include "Filters.hpp"
#include "ReleasesView.hpp"
#include "ReleaseView.hpp"
#include "SearchView.hpp"
#include "TrackListsView.hpp"
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
		IdxTracklists,
		IdxReleases,
		IdxRelease,
		IdxSearch,
		IdxTracks,
	};

	static const std::map<std::string, int> indexes =
	{
		{ "/artists",		IdxArtists },
		{ "/artist",		IdxArtist },
		{ "/tracklists",	IdxTracklists },
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
	Wt::WStackedWidget* contentsStack {bindNew<Wt::WStackedWidget>("contents")};
	contentsStack->setOverflow(Wt::Overflow::Visible); // wt makes it hidden by default

	// same order as enum Idx
	auto artists = std::make_unique<Artists>(*_filters);
	contentsStack->addWidget(std::move(artists));

	auto artist = std::make_unique<Artist>(_filters);
	artist->artistsAction.connect(this, &Explore::handleArtistsAction);
	artist->tracksAction.connect(this, &Explore::handleTracksAction);
	contentsStack->addWidget(std::move(artist));

	auto tracklists {std::make_unique<TrackLists>(*_filters)};
	tracklists->trackListAction.connect(this, &Explore::handleTrackListAction);
	contentsStack->addWidget(std::move(tracklists));

	auto releases = std::make_unique<Releases>(*_filters);
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

	auto tracks = std::make_unique<Tracks>(*_filters);
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
std::vector<Database::TrackId>
getArtistsTracks(Database::Session& session, const std::vector<Database::ArtistId>& artistsId, const std::vector<Database::ClusterId>& clusters, std::size_t maxTrackCount)
{
	assert(maxTrackCount);

	std::vector<Database::TrackId> res;

	auto transaction {session.createSharedTransaction()};

	for (const Database::ArtistId artistId : artistsId)
	{
		Database::Track::FindParameters params;
		params.setArtist(artistId);
		params.setSortMethod(Database::TrackSortMethod::DateDescAndRelease);
		params.setClusters(clusters);
		params.setRange({0, maxTrackCount - res.size()});

		const auto tracks {Database::Track::find(session, params)};

		res.reserve(res.size() + tracks.results.size());
		res.insert(std::end(res), std::cbegin(tracks.results), std::cend(tracks.results));

		if (res.size() == maxTrackCount)
			break;
	}

	return res;
}

static
std::vector<Database::TrackId>
getReleasesTracks(Database::Session& session, const std::vector<Database::ReleaseId>& releasesId, const std::vector<Database::ClusterId>& clusters, std::size_t maxTrackCount)
{
	using namespace Database;
	assert(maxTrackCount);

	std::vector<TrackId> res;

	auto transaction {session.createSharedTransaction()};

	for (const ReleaseId releaseId : releasesId)
	{
		Database::Track::FindParameters params;
		params.setRelease(releaseId);
		params.setSortMethod(Database::TrackSortMethod::Release);
		params.setClusters(clusters);
		params.setRange({0, maxTrackCount - res.size()});

		const auto tracks {Database::Track::find(session, params)};

		res.reserve(res.size() + tracks.results.size());
		res.insert(std::end(res), std::cbegin(tracks.results), std::cend(tracks.results));

		if (res.size() == maxTrackCount)
			break;
	}

	return res;
}

static
std::vector<Database::TrackId>
getTrackListTracks(Database::Session& session, Database::TrackListId trackListId, const std::vector<Database::ClusterId>& clusters, std::size_t maxTrackCount)
{
	using namespace Database;
	assert(maxTrackCount);

	auto transaction {session.createSharedTransaction()};

	Database::Track::FindParameters params;
	params.setTrackList(trackListId);
	params.setClusters(clusters);
	params.setRange({0, maxTrackCount});
	params.setSortMethod(TrackSortMethod::TrackList);

	return Database::Track::find(session, params).results;
}

void
Explore::handleArtistsAction(PlayQueueAction action, const std::vector<Database::ArtistId>& artistsId)
{
	tracksAction.emit(action, getArtistsTracks(LmsApp->getDbSession(), artistsId, _filters->getClusterIds(), _maxTrackCount));
}

void
Explore::handleReleasesAction(PlayQueueAction action, const std::vector<Database::ReleaseId>& releasesId)
{
	tracksAction.emit(action, getReleasesTracks(LmsApp->getDbSession(), releasesId, _filters->getClusterIds(), _maxTrackCount));
}

void
Explore::handleTrackListAction(PlayQueueAction action, Database::TrackListId trackListId)
{
	tracksAction.emit(action, getTrackListTracks(LmsApp->getDbSession(), trackListId, _filters->getClusterIds(), _maxTrackCount));
}

void
Explore::handleTracksAction(PlayQueueAction action, const std::vector<Database::TrackId>& tracksId)
{
	// consider things are already filtered here, and _maxTrackCount honored playqueue side...
	tracksAction.emit(action, tracksId);
}


} // namespace UserInterface

