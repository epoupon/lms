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
#include "utils/Random.hpp"

namespace {

bool
isFileSupported(const std::filesystem::path& file, const std::vector<std::filesystem::path>& extensions)
{
	return (std::find(std::cbegin(extensions), std::cend(extensions), file.extension()) != std::cend(extensions));
}

} // namespace

namespace CoverArt {

std::unique_ptr<IGrabber> createGrabber(const std::filesystem::path& execPath)
{
	return std::make_unique<Grabber>(execPath);
}

Grabber::Grabber(const std::filesystem::path& execPath)
{
	init(execPath);
}

void
Grabber::setDefaultCover(const std::filesystem::path& p)
{
	_defaultCover = std::make_unique<Image>();
	if (!_defaultCover->load(p))
		throw LmsException("Cannot read default cover file '" + p.string() + "'");
}

static std::optional<Image>
getFromAvMediaFile(const Av::MediaFile& input)
{
	std::vector<Image> res;

	for (auto& picture : input.getAttachedPictures(2))
	{
		Image image;

		if (image.load(picture.data))
			return image;
		else
			LMS_LOG(COVER, ERROR) << "Cannot load embedded cover file in '" << input.getPath().string() << "'";
	}

	LMS_LOG(COVER, DEBUG) << "No cover found in media file '" << input.getPath().string() << "'";
	return std::nullopt;
}

std::optional<Image>
Grabber::getFromDirectory(const std::filesystem::path& p, std::string_view preferredFileName) const
{
	const std::multimap<std::string, std::filesystem::path> coverPaths {getCoverPaths(p)};

	auto tryLoadImage = [](const std::filesystem::path& p, Image& image)
	{
		if (!image.load(p))
		{
			LMS_LOG(COVER, ERROR) << "Cannot load image in file '" << p.string() << "'";
			return false;
		}

		return true;
	};

	auto tryLoadImageFromFilename = [&](std::string_view fileName, Image& image)
	{
		auto range {coverPaths.equal_range(std::string {fileName})};
		for (auto it {range.first}; it != range.second; ++it)
		{
			if (tryLoadImage(it->second, image))
				return true;
		}
		return false;
	};

	Image image;

	if (!preferredFileName.empty() && tryLoadImageFromFilename(preferredFileName, image))
		return image;

	for (std::string_view filename : _preferredFileNames)
	{
		if (tryLoadImageFromFilename(filename, image))
			return image;
	}

	// Just pick one
	for (const auto& [filename, coverPath] : coverPaths)
	{
		if (tryLoadImage(coverPath, image))
			return image;
	}

	return std::nullopt;
}

std::multimap<std::string, std::filesystem::path>
Grabber::getCoverPaths(const std::filesystem::path& directoryPath) const
{
	std::multimap<std::string, std::filesystem::path> res;
	std::error_code ec;

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

		res.emplace(std::filesystem::path{path}.filename().replace_extension("").string(), path);
	}

	return res;
}

std::optional<Image>
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

Image
Grabber::getFromTrack(Database::Session& dbSession, Database::IdType trackId, std::size_t size)
{
	using namespace Database;

	const CacheEntryDesc cacheEntryDesc {CacheEntryDesc::Type::Track, trackId, size};
	std::optional<Image> cover {loadFromCache(cacheEntryDesc)};

	if (cover)
		return *cover;

	bool hasCover {};
	bool isMultiDisc {};
	std::filesystem::path trackPath;

	{
		auto transaction {dbSession.createSharedTransaction()};

		const Track::pointer track {Track::getById(dbSession, trackId)};
		if (track)
		{
			hasCover = track->hasCover();
			trackPath = track->getPath();

			auto release {track->getRelease()};
			if (release && release->getTotalDisc() > 1)
				isMultiDisc = true;
		}
	}

	if (hasCover)
		cover = getFromTrack(trackPath);

	if (!cover)
		cover = getFromDirectory(trackPath.parent_path(), trackPath.filename().replace_extension("").string());

	if (!cover && isMultiDisc)
	{
		if (trackPath.parent_path().has_parent_path())
			cover = getFromDirectory(trackPath.parent_path().parent_path(), {});
	}

	if (!cover)
		cover = *_defaultCover;

	cover->scale(Geometry {size, size});

	saveToCache(cacheEntryDesc, *cover);

	return *cover;
}


Image
Grabber::getFromRelease(Database::Session& session, Database::IdType releaseId, std::size_t size)
{
	const CacheEntryDesc cacheEntryDesc {CacheEntryDesc::Type::Release, releaseId, size};
	std::optional<Image> cover {loadFromCache(cacheEntryDesc)};

	if (cover)
		return *cover;

	std::optional<Database::IdType> trackId;
	{
		auto transaction {session.createSharedTransaction()};

		const auto release {Database::Release::getById(session, releaseId)};
		if (release)
		{
			const auto tracks {release->getTracks()};
			if (!tracks.empty())
				trackId = tracks.front().id();
		}
	}

	if (trackId)
	{
		cover = getFromTrack(session, *trackId, size);
	}
	else
	{
		if (!cover)
			cover = *_defaultCover;

		cover->scale(Geometry {size, size});
	}

	saveToCache(cacheEntryDesc, *cover);

	return *cover;
}

void
Grabber::flushCache()
{
	std::unique_lock lock {_cacheMutex};

	LMS_LOG(COVER, DEBUG) << "Cache stats: hits = " << _cacheHits << ", misses = " << _cacheMisses;
	_cacheHits = 0;
	_cacheMisses = 0;
	_cache.clear();
}

std::vector<uint8_t>
Grabber::getFromTrack(Database::Session& session, Database::IdType trackId, Format format, std::size_t width)
{
	const Image cover {getFromTrack(session, trackId, width)};

	assert(format == Format::JPEG);
	return cover.save(format);
}

std::vector<uint8_t>
Grabber::getFromRelease(Database::Session& session, Database::IdType releaseId, Format format, std::size_t width)
{
	const Image cover {getFromRelease(session, releaseId, width)};

	assert(format == Format::JPEG);
	return cover.save(format);
}

void
Grabber::saveToCache(const CacheEntryDesc& entryDesc, const Image& image)
{
	std::unique_lock lock {_cacheMutex};

	if (_cache.size() >= _maxCacheEntries)
		_cache.erase(Random::pickRandom(_cache));

	_cache[entryDesc] = image;
}

std::optional<Image>
Grabber::loadFromCache(const CacheEntryDesc& entryDesc)
{
	std::shared_lock lock {_cacheMutex};

	auto it {_cache.find(entryDesc)};
	if (it == std::cend(_cache))
	{
		++_cacheMisses;
		return std::nullopt;
	}

	++_cacheHits;
	return it->second;
}

} // namespace CoverArt

