/*
 * Copyright (C) 2013 Emeric Poupon
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

#include "logger/Logger.hpp"
#include "av/AvInfo.hpp"

#include "CoverArtGrabber.hpp"

namespace {

bool
isFileSupported(const boost::filesystem::path& file, const std::vector<boost::filesystem::path> extensions)
{
	boost::filesystem::path fileExtension = file.extension();

	for (auto extension : extensions)
	{
		if (extension == fileExtension)
			return true;
	}

	return false;
}
} // namespace

namespace CoverArt {

Grabber::Grabber()
{
}

Grabber&
Grabber::instance()
{
	static Grabber instance;
	return instance;
}

static std::vector<Image::Image>
getFromAvMediaFile(const Av::MediaFile& input, std::size_t nbMaxCovers)
{
	std::vector<Image::Image> res;

	for (Av::Picture& picture : input.getAttachedPictures(nbMaxCovers))
	{
		Image::Image image;

		if (image.load(picture.data))
			res.push_back( image );
		else
			LMS_LOG(COVER, ERROR) << "Cannot load embedded cover file in '" << input.getPath() << "'";
	}

	return res;
}

std::vector<Image::Image>
Grabber::getFromDirectory(const boost::filesystem::path& p, std::size_t nbMaxCovers) const
{
	std::vector<Image::Image> res;

	std::vector<boost::filesystem::path> coverPathes = getCoverPaths(p, nbMaxCovers);
	for (auto coverPath : coverPathes)
	{
		if (res.size() >= nbMaxCovers)
			break;

		Image::Image image;

		if (image.load(coverPath))
			res.push_back(image);
		else
			LMS_LOG(COVER, ERROR) << "Cannot load image in file '" << coverPath << "'";
	}

	return res;
}

std::vector<boost::filesystem::path>
Grabber::getCoverPaths(const boost::filesystem::path& directoryPath, std::size_t nbMaxCovers) const
{
	std::vector<boost::filesystem::path> res;

	// TODO handle preferred file names

	boost::filesystem::directory_iterator itPath(directoryPath);
	boost::filesystem::directory_iterator itEnd;
	while (itPath != itEnd)
	{
		boost::filesystem::path path = *itPath;
		itPath++;

		if (!boost::filesystem::is_regular(path))
			continue;

		if (!isFileSupported(path, _fileExtensions))
			continue;

		if (boost::filesystem::file_size(path) > _maxFileSize)
		{
			LMS_LOG(COVER, INFO) << "Cover file '" << path << " is too big (" << boost::filesystem::file_size(path) << "), limit is " << _maxFileSize;
			continue;
		}

		res.push_back(path);
		if (res.size() >= nbMaxCovers)
			break;
	}

	return res;
}

std::vector<Image::Image>
Grabber::getFromTrack(const boost::filesystem::path& p, std::size_t nbMaxCovers) const
{
	Av::MediaFile input(p);

	if (input.open())
		return getFromAvMediaFile(input, nbMaxCovers);
	else
		return std::vector<Image::Image>();
}

std::vector<Image::Image>
Grabber::getFromTrack(Wt::Dbo::Session& session, Database::Track::id_type trackId, std::size_t nbMaxCovers) const
{
	using namespace Database;

	Wt::Dbo::Transaction transaction(session);

	Track::pointer track = Track::getById(session, trackId);
	if (!track)
		return std::vector<Image::Image>();

	Track::CoverType coverType = track->getCoverType();
	boost::filesystem::path trackPath = track->getPath();

	transaction.commit();

	switch (coverType)
	{
		case Track::CoverType::Embedded:
			return Grabber::getFromTrack(trackPath, nbMaxCovers);
		case Track::CoverType::None:
			return Grabber::getFromDirectory(trackPath.parent_path(), nbMaxCovers);
	}

	return std::vector<Image::Image>();
}


std::vector<Image::Image>
Grabber::getFromRelease(Wt::Dbo::Session& session, Database::Release::id_type releaseId, std::size_t nbMaxCovers) const
{
	using namespace Database;

	Wt::Dbo::Transaction transaction(session);

	// If the release does not exist or is the special release "None", do nothing
	Release::pointer release = Release::getById(session, releaseId);
	if (!release || release->isNone())
		return std::vector<Image::Image>();

	// Get the first track of the release
	std::vector<Track::pointer> tracks = Track::getByFilter(session,
				SearchFilter::ById(SearchFilter::Field::Release, releaseId),
				-1, 1 /* limit result size */);

	if (tracks.empty())
		return std::vector<Image::Image>();

	boost::filesystem::path firstTrackPath = tracks.front()->getPath();
	bool embeddedCover = (tracks.front()->getCoverType() == Track::CoverType::Embedded);

	transaction.commit();

	// First, try to get covers from the directory of the release
	std::vector<Image::Image> res = getFromDirectory( firstTrackPath.parent_path(), nbMaxCovers);

	// Fallback on the embedded cover of the first track
	if (res.empty() && embeddedCover)
		res = getFromTrack( firstTrackPath, nbMaxCovers);

	return res;
}

} // namespace CoverArt

