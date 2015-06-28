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


#include <boost/foreach.hpp>

#include "logger/Logger.hpp"
#include "av/InputFormatContext.hpp"

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
: _maxFileSize(0)
{
}

Grabber&
Grabber::instance()
{
	static Grabber instance;
	return instance;
}

void
Grabber::init(const Config& config)
{
	for (auto extension : config.fileExtensions)
		_fileExtensions.push_back("." + extension);

	_maxFileSize = config.maxFileSize;
}

std::vector<CoverArt>
Grabber::getFromInputFormatContext(const Av::InputFormatContext& input, std::size_t nbMaxCovers) const
{
	std::vector<CoverArt> res;

	try
	{
		std::vector<Av::Picture> pictures = input.getPictures(nbMaxCovers);

		BOOST_FOREACH(const Av::Picture& picture, pictures)
			res.push_back( CoverArt(picture.mimeType, picture.data) );

	}
	catch(std::exception& e)
	{
		LMS_LOG(MOD_COVER, SEV_ERROR) << "Cannot get pictures: " << e.what();
	}

	return res;
}

std::vector<CoverArt>
Grabber::getFromDirectory(const boost::filesystem::path& p, std::size_t nbMaxCovers) const
{
	std::vector<CoverArt> res;

	std::vector<boost::filesystem::path> coverPathes = getCoverPaths(p, nbMaxCovers);
	for (auto coverPath : coverPathes)
	{
		if (res.size() >= nbMaxCovers)
			break;

		std::vector<unsigned char> data;
		std::ifstream file(coverPath.string(), std::ios::binary);
		char c;
		while (file.get(c))
			data.push_back(c);

		// TODO handle other formats
		res.push_back(CoverArt("image/jpeg", data));
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
			LMS_LOG(MOD_COVER, SEV_INFO) << "Cover file '" << path << " is too big (" << boost::filesystem::file_size(path) << "), limit is " << _maxFileSize;
			continue;
		}

		res.push_back(path);
		if (res.size() >= nbMaxCovers)
			break;
	}

	return res;
}

std::vector<CoverArt>
Grabber::getFromTrack(const boost::filesystem::path& p, std::size_t nbMaxCovers) const
{
	std::vector<CoverArt> res;

	try
	{
		Av::InputFormatContext input(p);

		res = getFromInputFormatContext(input, nbMaxCovers);

	}
	catch(std::exception& e)
	{
		LMS_LOG(MOD_COVER, SEV_ERROR) << "Cannot get covers from file " << p << ": " << e.what();
	}

	return res;
}

std::vector<CoverArt>
Grabber::getFromTrack(Wt::Dbo::Session& session, Database::Track::id_type trackId, std::size_t nbMaxCovers) const
{
	using namespace Database;

	Wt::Dbo::Transaction transaction(session);

	Track::pointer track = Track::getById(session, trackId);
	if (!track)
		return std::vector<CoverArt>();

	Track::CoverType coverType = track->getCoverType();
	boost::filesystem::path trackPath = track->getPath();

	transaction.commit();

	switch (coverType)
	{
		case Track::CoverType::Embedded:
			return Grabber::getFromTrack(trackPath, nbMaxCovers);
		case Track::CoverType::ExternalFile:
			return Grabber::getFromDirectory(trackPath.parent_path(), nbMaxCovers);
		case Track::CoverType::None:
			return std::vector<CoverArt>();
	}

	return std::vector<CoverArt>();
}


std::vector<CoverArt>
Grabber::getFromRelease(Wt::Dbo::Session& session, std::string releaseName, std::size_t nbMaxCovers) const
{
	using namespace Database;

	boost::filesystem::path firstTrackPath;
	bool embeddedCover = false;

	// Get the first track of the release
	{
		SearchFilter filter;
		filter.exactMatch[SearchFilter::Field::Release].push_back(releaseName);

		Wt::Dbo::Transaction transaction(session);

		std::vector<Track::pointer> tracks
			= Track::getAll(session, filter, -1, 1 /* limit result size */);

		if (tracks.empty())
			return std::vector<CoverArt>();

		firstTrackPath = tracks.front()->getPath();
		embeddedCover = (tracks.front()->getCoverType() == Track::CoverType::Embedded);
	}

	// First, try to get covers from the directory of the release
	std::vector<CoverArt> res = getFromDirectory( firstTrackPath.parent_path(), nbMaxCovers);

	// Fallback on the embedded cover of the first track
	if (res.empty() && embeddedCover)
		res = getFromTrack( firstTrackPath, nbMaxCovers);

	return res;
}

} // namespace CoverArt

