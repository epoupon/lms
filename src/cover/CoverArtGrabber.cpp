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

#include "CoverArtGrabber.hpp"

#include <boost/filesystem.hpp>

#include "av/AvInfo.hpp"

#include "database/Release.hpp"
#include "database/Track.hpp"

#include "utils/Logger.hpp"

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
	if (!_defaultCover.load( Wt::WApplication::instance()->docRoot() + "/images/unknown-cover.jpg"))
		throw LmsException("Cannot read default cover file");
}

Grabber&
Grabber::instance()
{
	static Grabber instance;
	return instance;
}

static boost::optional<Image::Image>
getFromAvMediaFile(const Av::MediaFile& input)
{
	std::vector<Image::Image> res;

	for (auto& picture : input.getAttachedPictures(2))
	{
		Image::Image image;

		if (image.load(picture.data))
			return image;
		else
			LMS_LOG(COVER, ERROR) << "Cannot load embedded cover file in '" << input.getPath().string() << "'";
	}

	LMS_LOG(COVER, DEBUG) << "No cover found in media file '" << input.getPath().string() << "'";
	return boost::none;
}

boost::optional<Image::Image>
Grabber::getFromDirectory(const boost::filesystem::path& p) const
{
	for (auto coverPath : getCoverPaths(p))
	{
		Image::Image image;

		if (image.load(coverPath))
			return image;
		else
			LMS_LOG(COVER, ERROR) << "Cannot load image in file '" << coverPath.string() << "'";
	}

	LMS_LOG(COVER, DEBUG) << "No cover found in directory '" << p.string() << "'";
	return boost::none;
}

std::vector<boost::filesystem::path>
Grabber::getCoverPaths(const boost::filesystem::path& directoryPath) const
{
	std::vector<boost::filesystem::path> res;
	boost::system::error_code ec;

	// TODO handle preferred file names

	boost::filesystem::directory_iterator itPath(directoryPath, ec);
	boost::filesystem::directory_iterator itEnd;
	while (!ec && itPath != itEnd)
	{
		boost::filesystem::path path = *itPath;
		itPath.increment(ec);

		if (!boost::filesystem::is_regular(path))
			continue;

		if (!isFileSupported(path, _fileExtensions))
			continue;

		if (boost::filesystem::file_size(path) > _maxFileSize)
		{
			LMS_LOG(COVER, INFO) << "Cover file '" << path.string() << " is too big (" << boost::filesystem::file_size(path) << "), limit is " << _maxFileSize;
			continue;
		}

		res.push_back(path);
	}

	return res;
}

boost::optional<Image::Image>
Grabber::getFromTrack(const boost::filesystem::path& p) const
{
	try
	{
		Av::MediaFile input(p);

		return getFromAvMediaFile(input);
	}
	catch (Av::MediaFileException& e)
	{
		LMS_LOG(COVER, ERROR) << "Cannot get covers from track " << p.string() << ": " << e.what();
		return boost::none;
	}
}

Image::Image
Grabber::getFromTrack(Wt::Dbo::Session& session, Database::IdType trackId, std::size_t size) const
{
	using namespace Database;

	boost::optional<Image::Image> cover;

	{
		Wt::Dbo::Transaction transaction(session);

		Track::pointer track = Track::getById(session, trackId);
		if (track)
		{
			Track::CoverType coverType = track->getCoverType();
			boost::filesystem::path trackPath = track->getPath();

			transaction.commit();

			if (coverType == Track::CoverType::Embedded)
				cover = getFromTrack(trackPath);

			if (!cover)
				cover = getFromDirectory(trackPath.parent_path());
		}
	}

	if (!cover)
		cover = _defaultCover;

	cover->scale(size);

	return *cover;
}


Image::Image
Grabber::getFromRelease(Wt::Dbo::Session& session, Database::IdType releaseId, std::size_t size) const
{
	using namespace Database;

	boost::optional<Image::Image> cover;

	{
		Wt::Dbo::Transaction transaction(session);

		auto release = Release::getById(session, releaseId);
		if (release)
		{
			auto tracks = release->getTracks();
			if (!tracks.empty())
			{
				auto trackId = tracks.front().id();
				transaction.commit();

				return getFromTrack(session, trackId, size);
			}
		}
	}

	if (!cover)
		cover = _defaultCover;

	cover->scale(size);

	return *cover;
}

std::vector<uint8_t>
Grabber::getFromTrack(Wt::Dbo::Session& session, Database::IdType trackId, Image::Format format, std::size_t size) const
{
	Image::Image cover = getFromTrack(session, trackId, size);

	return cover.save(Image::Format::JPEG);
}

std::vector<uint8_t>
Grabber::getFromRelease(Wt::Dbo::Session& session, Database::IdType releaseId, Image::Format format, std::size_t size) const
{
	Image::Image cover = getFromRelease(session, releaseId, size);

	return cover.save(Image::Format::JPEG);
}

} // namespace CoverArt

