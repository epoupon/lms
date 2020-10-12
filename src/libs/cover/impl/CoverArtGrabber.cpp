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

#include "CoverArt.hpp"

namespace {

bool
isFileSupported(const std::filesystem::path& file, const std::vector<std::filesystem::path>& extensions)
{
	return (std::find(std::cbegin(extensions), std::cend(extensions), file.extension()) != std::cend(extensions));
}

} // namespace

namespace CoverArt {

std::unique_ptr<IGrabber> createGrabber(const std::filesystem::path& execPath, std::size_t maxCacheSize, std::size_t maxFileSize)
{
	return std::make_unique<Grabber>(execPath, maxCacheSize, maxFileSize);
}

Grabber::Grabber(const std::filesystem::path& execPath,
		std::size_t maxCacheSize,
		std::size_t maxFileSize)
	: _maxCacheSize {maxCacheSize}
	, _maxFileSize {maxFileSize}
{
	LMS_LOG(COVER, INFO) << "Max cache size = " << _maxCacheSize;
	LMS_LOG(COVER, INFO) << "Max file size = " << _maxFileSize;
	init(execPath);
}

void
Grabber::setDefaultCover(const std::filesystem::path& p)
{
	try
	{
		RawImage defaultCover {p};
		_defaultCover = defaultCover.encode();
		LMS_LOG(COVER, INFO) << "Successfully read default cover image!";
	}
	catch (const ImageException& e)
	{
		throw LmsException("Cannot read default cover file '" + p.string() + "'");
	}
}

static std::optional<EncodedImage>
getFromAvMediaFile(const Av::MediaFile& input, Width width)
{
	std::optional<EncodedImage> image;

	input.visitAttachedPictures([&](const Av::Picture& picture)
	{
		if (image)
			return;

		try
		{
			EncodedImage encodedImage {picture.data, picture.dataSize};

			RawImage rawImage {encodedImage};
			rawImage.scale(width);

			image = rawImage.encode();
		}
		catch (const ImageException& e)
		{
			LMS_LOG(COVER, ERROR) << "Cannot read embedded cover: " << e.what();
		}
	});

	return image;
}

static std::optional<EncodedImage>
getFromFile(const std::filesystem::path& p, Width width)
{
	std::optional<EncodedImage> image;

	try
	{
		RawImage rawImage {p};
		rawImage.scale(width);

		image = rawImage.encode();
	}
	catch (const ImageException& e)
	{
		LMS_LOG(COVER, ERROR) << "Cannot read cover in file '" << p.string() << "': " << e.what();
	}

	return image;
}

EncodedImage
Grabber::getDefault(Width width)
{
	{
		std::shared_lock lock {_cacheMutex};

		if (auto it {_defaultCache.find(width)}; it != std::cend(_defaultCache))
			return it->second;
	}

	{
		std::unique_lock lock {_cacheMutex};

		if (auto it {_defaultCache.find(width)}; it != std::cend(_defaultCache))
			return it->second;

		RawImage rawImage {*_defaultCover};
		rawImage.scale(width);
		EncodedImage res {rawImage.encode()};

		_defaultCache[width] = res;
		LMS_LOG(COVER, DEBUG) << "Default cache entries = " << _defaultCache.size();

		return res;
	}
}

std::optional<EncodedImage>
Grabber::getFromDirectory(const std::filesystem::path& p, std::string_view preferredFileName, Width width) const
{
	const std::multimap<std::string, std::filesystem::path> coverPaths {getCoverPaths(p)};

	auto tryLoadImageFromFilename = [&](std::string_view fileName)
	{
		std::optional<EncodedImage> image;

		auto range {coverPaths.equal_range(std::string {fileName})};
		for (auto it {range.first}; it != range.second; ++it)
		{
			image = getFromFile(it->second, width);
			if (!image)
				continue;
		}
		return image;
	};

	std::optional<EncodedImage> image;

	if (!preferredFileName.empty())
	{
		image = tryLoadImageFromFilename(preferredFileName);
		if (image)
			return image;
	}

	for (std::string_view filename : _preferredFileNames)
	{
		image = tryLoadImageFromFilename(filename);
		if (image)
			return image;
	}

	// Just pick one
	for (const auto& [filename, coverPath] : coverPaths)
	{
		image = getFromFile(coverPath, width);
		if (image)
			return image;
	}

	return image;
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

std::optional<EncodedImage>
Grabber::getFromTrack(const std::filesystem::path& p, Width width) const
{
	std::optional<EncodedImage> image;

	try
	{
		Av::MediaFile input {p};

		image = getFromAvMediaFile(input, width);
	}
	catch (Av::AvException& e)
	{
		LMS_LOG(COVER, ERROR) << "Cannot get covers from track " << p.string() << ": " << e.what();
	}

	return image;
}

EncodedImage
Grabber::getFromTrackInternal(Database::Session& dbSession, Database::IdType trackId, Width width)
{
	using namespace Database;

	const CacheEntryDesc cacheEntryDesc {CacheEntryDesc::Type::Track, trackId, width};
	std::optional<EncodedImage> cover {loadFromCache(cacheEntryDesc)};

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
		cover = getFromTrack(trackPath, width);

	if (!cover)
		cover = getFromDirectory(trackPath.parent_path(), trackPath.filename().replace_extension("").string(), width);

	if (!cover && isMultiDisc)
	{
		if (trackPath.parent_path().has_parent_path())
			cover = getFromDirectory(trackPath.parent_path().parent_path(), {}, width);
	}

	if (!cover)
		cover = getDefault(width);

	saveToCache(cacheEntryDesc, *cover);

	return *cover;
}

EncodedImage
Grabber::getFromReleaseInternal(Database::Session& session, Database::IdType releaseId, Width width)
{
	const CacheEntryDesc cacheEntryDesc {CacheEntryDesc::Type::Release, releaseId, width};
	std::optional<EncodedImage> cover {loadFromCache(cacheEntryDesc)};

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
		cover = getFromTrackInternal(session, *trackId, width);
	}

	if (!cover)
		cover = getDefault(width);

	saveToCache(cacheEntryDesc, *cover);

	return *cover;
}

void
Grabber::flushCache()
{
	std::unique_lock lock {_cacheMutex};

	LMS_LOG(COVER, DEBUG) << "Cache stats: hits = " << _cacheHits << ", misses = " << _cacheMisses << ", nb entries = " << _cache.size() << ", size = " << _cacheSize;
	_cacheHits = 0;
	_cacheMisses = 0;
	_cacheSize = 0;
	_cache.clear();
}

std::unique_ptr<ICoverArt>
Grabber::getFromTrack(Database::Session& session, Database::IdType trackId, std::size_t width)
{
	CoverArt toto {getFromTrackInternal(session, trackId, width)};
	return std::make_unique<CoverArt>(getFromTrackInternal(session, trackId, width));
}

std::unique_ptr<ICoverArt>
Grabber::getFromRelease(Database::Session& session, Database::IdType releaseId, std::size_t width)
{
	return std::make_unique<CoverArt>(getFromReleaseInternal(session, releaseId, width));
}

void
Grabber::saveToCache(const CacheEntryDesc& entryDesc, const EncodedImage& image)
{
	std::unique_lock lock {_cacheMutex};

	while (_cacheSize + image.getDataSize() > _maxCacheSize && !_cache.empty())
	{
		auto it {Random::pickRandom(_cache)};
		_cacheSize -= it->second.getDataSize();
		_cache.erase(it);
	}

	_cacheSize += image.getDataSize();
	_cache[entryDesc] = image;
}

std::optional<EncodedImage>
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

