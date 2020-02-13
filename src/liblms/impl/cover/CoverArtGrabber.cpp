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

#include "av/AvInfo.hpp"

#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"

#include "utils/Logger.hpp"

namespace {

bool
isFileSupported(const std::filesystem::path& file, const std::vector<std::filesystem::path>& extensions)
{
	return (std::find(std::cbegin(extensions), std::cend(extensions), file.extension()) != std::cend(extensions));
}

} // namespace

namespace CoverArt {

Grabber::Grabber()
{
}

void
Grabber::setDefaultCover(const std::filesystem::path& p)
{
	if (!_defaultCover.load(p))
		throw LmsException("Cannot read default cover file '" + p.string() + "'");
}

Image::Image
Grabber::getDefaultCover(std::size_t size)
{
	LMS_LOG(COVER, DEBUG) << "Getting a default cover using size = " << size;
	std::unique_lock<std::mutex> lock(_mutex);

	auto it = _defaultCovers.find(size);
	if (it == _defaultCovers.end())
	{
		Image::Image cover = _defaultCover;

		LMS_LOG(COVER, DEBUG) << "default cover size = " << cover.getSize().width << " x " << cover.getSize().height;

		LMS_LOG(COVER, DEBUG) << "Scaling cover to size = " << size;
		cover.scale(Image::Geometry{size, size});
		LMS_LOG(COVER, DEBUG) << "Scaling DONE";
		auto res = _defaultCovers.insert(std::make_pair(size, cover));
		assert(res.second);
		it = res.first;
	}

	return it->second;
}

static std::optional<Image::Image>
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
	return std::nullopt;
}

std::optional<Image::Image>
Grabber::getFromDirectory(const std::filesystem::path& p) const
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
	return std::nullopt;
}

std::vector<std::filesystem::path>
Grabber::getCoverPaths(const std::filesystem::path& directoryPath) const
{
	std::vector<std::filesystem::path> res;
	std::error_code ec;

	// TODO handle preferred file names

	std::filesystem::directory_iterator itPath(directoryPath, ec);
	std::filesystem::directory_iterator itEnd;
	while (!ec && itPath != itEnd)
	{
		const std::filesystem::path path {*itPath};
		itPath.increment(ec);

		if (!std::filesystem::is_regular_file(path))
			continue;

		if (!isFileSupported(path, _fileExtensions))
			continue;

		if (std::filesystem::file_size(path) > _maxFileSize)
		{
			LMS_LOG(COVER, INFO) << "Cover file '" << path.string() << " is too big (" << std::filesystem::file_size(path) << "), limit is " << _maxFileSize;
			continue;
		}

		res.push_back(path);
	}

	return res;
}

std::optional<Image::Image>
Grabber::getFromTrack(const std::filesystem::path& p) const
{
	try
	{
		Av::MediaFile input(p);

		return getFromAvMediaFile(input);
	}
	catch (Av::MediaFileException& e)
	{
		LMS_LOG(COVER, ERROR) << "Cannot get covers from track " << p.string() << ": " << e.what();
		return std::nullopt;
	}
}

Image::Image
Grabber::getFromTrack(Database::Session& dbSession, Database::IdType trackId, std::size_t size)
{
	using namespace Database;

	std::optional<Image::Image> cover;

	bool hasCover {};
	bool isMultiDisc {};
	std::filesystem::path trackPath;

	{
		auto transaction {dbSession.createSharedTransaction()};

		Track::pointer track = Track::getById(dbSession, trackId);
		if (track)
		{
			hasCover = track->hasCover();
			trackPath = track->getPath();

			auto release {track->getRelease()};
			if (release && release->getTotalDiscNumber() > 1)
				isMultiDisc = true;
		}
	}

	if (hasCover)
		cover = getFromTrack(trackPath);

	if (!cover)
		cover = getFromDirectory(trackPath.parent_path());

	if (!cover && isMultiDisc)
	{
		if (trackPath.parent_path().has_parent_path())
			cover = getFromDirectory(trackPath.parent_path().parent_path());
	}

	if (!cover)
		cover = getDefaultCover(size);
	else
		cover->scale(Image::Geometry {size, size});

	return *cover;
}


Image::Image
Grabber::getFromRelease(Database::Session& session, Database::IdType releaseId, std::size_t size)
{
	std::optional<Image::Image> cover;

	std::optional<Database::IdType> trackId;
	{
		auto transaction {session.createSharedTransaction()};

		auto release {Database::Release::getById(session, releaseId)};
		if (release)
		{
			auto tracks {release->getTracks()};
			if (!tracks.empty())
				trackId = tracks.front().id();
		}
	}

	if (trackId)
		return getFromTrack(session, *trackId, size);

	if (!cover)
		cover = getDefaultCover(size);
	else
		cover->scale(Image::Geometry {size, size});

	return *cover;
}

std::vector<uint8_t>
Grabber::getFromTrack(Database::Session& session, Database::IdType trackId, Format format, std::size_t size)
{
	const Image::Image cover {getFromTrack(session, trackId, size)};

	assert(format == Image::Format::JPEG);
	return cover.save(format);
}

std::vector<uint8_t>
Grabber::getFromRelease(Database::Session& session, Database::IdType releaseId, Format format, std::size_t size)
{
	const Image::Image cover {getFromRelease(session, releaseId, size)};

	assert(format == Image::Format::JPEG);
	return cover.save(format);
}

} // namespace CoverArt

