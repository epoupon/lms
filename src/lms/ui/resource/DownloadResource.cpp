/*
 * Copyright (C) 2014 Emeric Poupon
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

#include "DownloadResource.hpp"

#include <array>
#include <iostream>
#include <iomanip>

#include <Wt/Http/Response.h>
#include <Wt/WDateTime.h>

#include "services/database/Artist.hpp"
#include "services/database/Release.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "services/database/TrackList.hpp"
#include "utils/Exception.hpp"
#include "utils/Logger.hpp"

#include "LmsApplication.hpp"

#define LOG(level)	LMS_LOG(UI, level) << "Download resource: "

namespace UserInterface {

DownloadResource::~DownloadResource()
{
	beingDeleted();
}

void
DownloadResource::handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response)
{
	try
	{
		std::shared_ptr<Zip::IZipper> zipper;

		// First, see if this request is for a continuation
		if (Wt::Http::ResponseContinuation *continuation {request.continuation()})
			zipper = Wt::cpp17::any_cast<std::shared_ptr<Zip::IZipper>>(continuation->data());
		else
		{
			zipper = createZipper();
			response.setMimeType("application/zip");
		}

		zipper->writeSome(response.out());
		if (!zipper->isComplete())
		{
			auto* continuation {response.createContinuation()};
			continuation->setData(zipper);
		}
	}
	catch (Zip::Exception& exception)
	{
		LOG(ERROR) << "Zipper exception: " << exception.what();
	}
}

static
std::string
getArtistPathName(Database::Artist::pointer artist)
{
	return StringUtils::replaceInString(artist->getName(), "/", "_");
}

static
std::string
getReleaseArtistPathName(Database::Release::pointer release)
{
	std::string releaseArtistName;

	std::vector<Database::ObjectPtr<Database::Artist>> artists;

	artists = release->getReleaseArtists();
	if (artists.empty())
		artists = release->getArtists();

	if (artists.size() > 1)
		releaseArtistName = Wt::WString::tr("Lms.Explore.various-artists").toUTF8();
	else if (artists.size() == 1)
		releaseArtistName = artists.front()->getName();

	releaseArtistName = StringUtils::replaceInString(releaseArtistName, "/", "_");

	return releaseArtistName;
}

static
std::string
getReleasePathName(Database::Release::pointer release)
{
	std::string releaseName;

	if (const Wt::WDate releaseDate {release->getReleaseDate()}; releaseDate.isValid())
		releaseName += std::to_string(releaseDate.year()) + " - ";
	releaseName += StringUtils::replaceInString(release->getName(), "/", "_");

	return releaseName;
}

namespace details
{
	static
	std::string
	getTrackPathName(Database::Track::pointer track)
	{
		std::ostringstream fileName;

		if (auto discNumber {track->getDiscNumber()})
			fileName << *discNumber << ".";
		if (auto trackNumber {track->getTrackNumber()})
			fileName << std::setw(2) << std::setfill('0') << *trackNumber << " - ";

		fileName << StringUtils::replaceInString(track->getName(), "/", "_") << track->getPath().filename().extension().string();

		return fileName.str();
	}

	static
	std::string
	getTrackListPathName(Database::TrackList::pointer trackList)
	{
		return StringUtils::replaceInString(trackList->getName(), "/", "_");
	}

	static
	std::unique_ptr<Zip::IZipper>
	createZipper(const std::vector<Database::Track::pointer>& tracks)
	{
		if (tracks.empty())
			return {};

		Zip::EntryContainer files;
		for (const Database::Track::pointer& track : tracks)
		{
			std::string releaseName;
			std::string releaseArtistName;
			if (auto release {track->getRelease()})
			{
				releaseName = getReleasePathName(release);
				releaseArtistName = getReleaseArtistPathName(release);
			}

			std::string fileName;
			if (!releaseArtistName.empty())
				fileName += releaseArtistName + "/";
			if (!releaseName.empty())
				fileName += releaseName + "/";
			fileName += getTrackPathName(track);

			files.emplace_back(Zip::Entry {fileName, track->getPath()});
		}

		return Zip::createArchiveZipper(files);
	}
}

DownloadArtistResource::DownloadArtistResource(Database::ArtistId artistId)
: _artistId {artistId}
{
	auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	Database::Artist::pointer artist {Database::Artist::find(LmsApp->getDbSession(), artistId)};
	if (artist)
		suggestFileName(getArtistPathName(artist) + ".zip");
}

std::unique_ptr<Zip::IZipper>
DownloadArtistResource::createZipper()
{
	auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	const auto trackResults {Database::Track::find(LmsApp->getDbSession(), Database::Track::FindParameters {}.setArtist(_artistId).setSortMethod(Database::TrackSortMethod::DateDescAndRelease))};
	std::vector<Database::Track::pointer> tracks;
	tracks.reserve(trackResults.results.size());

	for (const Database::TrackId trackId : trackResults.results)
		tracks.push_back(Database::Track::find(LmsApp->getDbSession(), trackId));

	return details::createZipper(tracks);
}

DownloadReleaseResource::DownloadReleaseResource(Database::ReleaseId releaseId)
: _releaseId {releaseId}
{
	auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	Database::Release::pointer release {Database::Release::find(LmsApp->getDbSession(), releaseId)};
	if (release)
		suggestFileName(getReleasePathName(release) + ".zip");
}


std::unique_ptr<Zip::IZipper>
DownloadReleaseResource::createZipper()
{
	using namespace Database;

	auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	auto trackResults {Track::find(LmsApp->getDbSession(), Track::FindParameters {}.setRelease(_releaseId).setSortMethod(TrackSortMethod::Release))};

	std::vector<Track::pointer> tracks;
	tracks.reserve(trackResults.results.size());
	std::transform(std::cbegin(trackResults.results), std::cend(trackResults.results), std::back_inserter(tracks), [](TrackId trackId){ return Track::find(LmsApp->getDbSession(), trackId); });

	return details::createZipper(tracks);
}

DownloadTrackResource::DownloadTrackResource(Database::TrackId trackId)
: _trackId {trackId}
{
	auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	Database::Track::pointer track {Database::Track::find(LmsApp->getDbSession(), trackId)};
	if (track)
		suggestFileName(details::getTrackPathName(track) + ".zip");
}

std::unique_ptr<Zip::IZipper>
DownloadTrackResource::createZipper()
{
	auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	const Database::Track::pointer track {Database::Track::find(LmsApp->getDbSession(), _trackId)};
	if (!track)
	{
		LOG(DEBUG) << "Cannot find track";
		return {};
	}

	return details::createZipper({track});
}

DownloadTrackListResource::DownloadTrackListResource(Database::TrackListId trackListId)
	: _trackListId {trackListId}
{
	auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	const Database::TrackList::pointer trackList {Database::TrackList::find(LmsApp->getDbSession(), trackListId)};
	if (trackList)
		suggestFileName(details::getTrackListPathName(trackList) + ".zip");
}

std::unique_ptr<Zip::IZipper>
DownloadTrackListResource::createZipper()
{
	using namespace Database;
	auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	Track::FindParameters params;
	params.setTrackList(_trackListId);
	const auto trackResults {Track::find(LmsApp->getDbSession(), params)};

	std::vector<Track::pointer> tracks;
	tracks.reserve(trackResults.results.size());
	std::transform(std::cbegin(trackResults.results), std::cend(trackResults.results), std::back_inserter(tracks), [](TrackId trackId){ return Track::find(LmsApp->getDbSession(), trackId); });

	return details::createZipper(tracks);
}

} // namespace UserInterface
