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

#if LMS_SUPPORT_IMAGE_STB
#include "stb/RawImage.hpp"
using RawImage = CoverArt::STB::RawImage;
#elif LMS_SUPPORT_IMAGE_GM
#include "graphicsmagick/RawImage.hpp"
using RawImage = CoverArt::GraphicsMagick::RawImage;
#endif

#include "utils/Logger.hpp"
#include "utils/Random.hpp"
#include "utils/Utils.hpp"
#include "Exception.hpp"

namespace
{
	struct TrackInfo
	{
		bool hasCover {};
		bool isMultiDisc {};
		std::filesystem::path trackPath;
		std::optional<Database::IdType> releaseId;
	};

	std::optional<TrackInfo>
	getTrackInfo(Database::Session& dbSession, Database::IdType trackId)
	{
		std::optional<TrackInfo> res;

		auto transaction {dbSession.createSharedTransaction()};

		const Database::Track::pointer track {Database::Track::getById(dbSession, trackId)};
		if (!track)
			return res;

		res = TrackInfo {};

		res->hasCover = track->hasCover();
		res->trackPath = track->getPath();

		if (const Database::Release::pointer& release {track->getRelease()})
		{
			res->releaseId = release.id();
			if (release->getTotalDisc() > 1)
				res->isMultiDisc = true;
		}

		return res;
	}
}


namespace CoverArt {

static
bool
isFileSupported(const std::filesystem::path& file, const std::vector<std::filesystem::path>& extensions)
{
	return (std::find(std::cbegin(extensions), std::cend(extensions), file.extension()) != std::cend(extensions));
}

std::unique_ptr<IGrabber>
createGrabber(const std::filesystem::path& execPath,
		const std::filesystem::path& defaultCoverPath,
		std::size_t maxCacheSize, std::size_t maxFileSize, unsigned jpegQuality)
{
	return std::make_unique<Grabber>(execPath, defaultCoverPath, maxCacheSize, maxFileSize, jpegQuality);
}

Grabber::Grabber(const std::filesystem::path& execPath,
		const std::filesystem::path& defaultCoverPath,
		std::size_t maxCacheSize,
		std::size_t maxFileSize,
		unsigned jpegQuality)
	: _defaultCoverPath {defaultCoverPath}
	, _maxCacheSize {maxCacheSize}
	, _maxFileSize {maxFileSize}
	, _jpegQuality {clamp<unsigned>(jpegQuality, 1, 100)}
{
	LMS_LOG(COVER, INFO) << "Default cover path = '" << _defaultCoverPath.string() << "'";
	LMS_LOG(COVER, INFO) << "Max cache size = " << _maxCacheSize;
	LMS_LOG(COVER, INFO) << "Max file size = " << _maxFileSize;
	LMS_LOG(COVER, INFO) << "JPEG export quality = " << _jpegQuality;

#if LMS_SUPPORT_IMAGE_GM
	GraphicsMagick::init(execPath);
#else
	(void)execPath;
#endif

	try
	{
		getDefault(512);
	}
	catch (const ImageException& e)
	{
		throw LmsException("Cannot read default cover file '" + _defaultCoverPath.string() + "': " + e.what());
	}
}

std::unique_ptr<IEncodedImage>
Grabber::getFromAvMediaFile(const Av::MediaFile& input, ImageSize width) const
{
	std::unique_ptr<IEncodedImage> image;

	input.visitAttachedPictures([&](const Av::Picture& picture)
	{
		if (image)
			return;

		try
		{
			RawImage rawImage {picture.data, picture.dataSize};
			rawImage.resize(width);
			image = rawImage.encodeToJPEG(_jpegQuality);
		}
		catch (const ImageException& e)
		{
			LMS_LOG(COVER, ERROR) << "Cannot read embedded cover: " << e.what();
		}
	});

	return image;
}

std::unique_ptr<IEncodedImage>
Grabber::getFromCoverFile(const std::filesystem::path& p, ImageSize width) const
{
	std::unique_ptr<IEncodedImage> image;

	try
	{
		RawImage rawImage {p};
		rawImage.resize(width);
		image = rawImage.encodeToJPEG(_jpegQuality);
	}
	catch (const ImageException& e)
	{
		LMS_LOG(COVER, ERROR) << "Cannot read cover in file '" << p.string() << "': " << e.what();
	}

	return image;
}

std::shared_ptr<IEncodedImage>
Grabber::getDefault(ImageSize width)
{
	{
		std::shared_lock lock {_cacheMutex};

		if (auto it {_defaultCoverCache.find(width)}; it != std::cend(_defaultCoverCache))
			return it->second;
	}

	{
		std::unique_lock lock {_cacheMutex};

		if (auto it {_defaultCoverCache.find(width)}; it != std::cend(_defaultCoverCache))
			return it->second;

		std::shared_ptr<IEncodedImage> image {getFromCoverFile(_defaultCoverPath, width)};
		_defaultCoverCache[width] = image;
		LMS_LOG(COVER, DEBUG) << "Default cache entries = " << _defaultCoverCache.size();

		return image;
	}
}

std::unique_ptr<IEncodedImage>
Grabber::getFromDirectory(const std::filesystem::path& directory, ImageSize width) const
{
	const std::multimap<std::string, std::filesystem::path> coverPaths {getCoverPaths(directory)};

	auto tryLoadImageFromFilename = [&](std::string_view fileName)
	{
		std::unique_ptr<IEncodedImage> image;

		auto range {coverPaths.equal_range(std::string {fileName})};
		for (auto it {range.first}; it != range.second; ++it)
		{
			image = getFromCoverFile(it->second, width);
			if (image)
				break;
		}
		return image;
	};

	std::unique_ptr<IEncodedImage> image;

	for (std::string_view filename : _preferredFileNames)
	{
		image = tryLoadImageFromFilename(filename);
		if (image)
			return image;
	}

	// Just pick one
	for (const auto& [filename, coverPath] : coverPaths)
	{
		image = getFromCoverFile(coverPath, width);
		if (image)
			return image;
	}

	return image;
}

std::unique_ptr<IEncodedImage>
Grabber::getFromSameNamedFile(const std::filesystem::path& filePath, ImageSize width) const
{
	std::unique_ptr<IEncodedImage> res;

	std::filesystem::path coverPath {filePath};
	for (const std::filesystem::path& extension : _fileExtensions)
	{
		coverPath.replace_extension(extension);

		if (!checkCoverFile(coverPath))
			continue;

		res = getFromCoverFile(coverPath, width);
		if (res)
			break;
	}

	return res;
}

bool
Grabber::checkCoverFile(const std::filesystem::path& filePath) const
{
	std::error_code ec;

	if (!isFileSupported(filePath, _fileExtensions))
		return false;

	if (!std::filesystem::exists(filePath, ec))
		return false;

	if (!std::filesystem::is_regular_file(filePath, ec))
		return false;

	if (std::filesystem::file_size(filePath, ec) > _maxFileSize && !ec)
	{
		LMS_LOG(COVER, INFO) << "Cover file '" << filePath.string() << " is too big (" << std::filesystem::file_size(filePath, ec) << "), limit is " << _maxFileSize;
		return false;
	}

	return true;
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
		const std::filesystem::path& path {*itPath};

		if (checkCoverFile(path))
			res.emplace(std::filesystem::path{ path }.filename().replace_extension("").string(), path);

		itPath.increment(ec);
	}

	return res;
}

std::unique_ptr<IEncodedImage>
Grabber::getFromTrack(const std::filesystem::path& p, ImageSize width) const
{
	std::unique_ptr<IEncodedImage> image;

	try
	{
		const Av::MediaFile input {p};
		image = getFromAvMediaFile(input, width);
	}
	catch (Av::AvException& e)
	{
		LMS_LOG(COVER, ERROR) << "Cannot get covers from track " << p.string() << ": " << e.what();
	}

	return image;
}

std::shared_ptr<IEncodedImage>
Grabber::getFromTrack(Database::Session& dbSession, Database::IdType trackId, ImageSize width)
{
	return getFromTrack(dbSession, trackId, width, true /* allow release fallback*/);
}




std::shared_ptr<IEncodedImage>
Grabber::getFromTrack(Database::Session& dbSession, Database::IdType trackId, ImageSize width, bool allowReleaseFallback)
{
	using namespace Database;

	const CacheEntryDesc cacheEntryDesc {CacheEntryDesc::Type::Track, trackId, width};

	std::shared_ptr<IEncodedImage> cover {loadFromCache(cacheEntryDesc)};
	if (cover)
		return cover;

	if (const std::optional<TrackInfo> trackInfo {getTrackInfo(dbSession, trackId)})
	{
		if (trackInfo->hasCover)
			cover = getFromTrack(trackInfo->trackPath, width);

		if (!cover)
			cover = getFromSameNamedFile(trackInfo->trackPath, width);

		if (!cover && trackInfo->releaseId && allowReleaseFallback)
			cover = getFromRelease(dbSession, *trackInfo->releaseId, width);

		if (!cover && trackInfo->isMultiDisc)
		{
			if (trackInfo->trackPath.parent_path().has_parent_path())
				cover = getFromDirectory(trackInfo->trackPath.parent_path().parent_path(), width);
		}
	}

	if (!cover)
		cover = getDefault(width);

	if (cover)
		saveToCache(cacheEntryDesc, cover);

	return cover;
}

std::shared_ptr<IEncodedImage>
Grabber::getFromRelease(Database::Session& session, Database::IdType releaseId, ImageSize width)
{
	const CacheEntryDesc cacheEntryDesc {CacheEntryDesc::Type::Release, releaseId, width};

	std::shared_ptr<IEncodedImage> cover {loadFromCache(cacheEntryDesc)};
	if (cover)
		return cover;

	struct ReleaseInfo
	{
		Database::IdType firstTrackId;
		std::filesystem::path releaseDirectory;
	};

	auto getReleaseInfo {[&]
	{
		std::optional<ReleaseInfo> res;

		auto transaction {session.createSharedTransaction()};

		if (const Database::Release::pointer release {Database::Release::getById(session, releaseId)})
		{
			if (const auto firstTrack {release->getFirstTrack()})
			{
				res = ReleaseInfo {};
				res->firstTrackId = firstTrack.id();
				res->releaseDirectory = firstTrack->getPath().parent_path();
			}
		}

		return res;
	}};

	if (const std::optional<ReleaseInfo> releaseInfo {getReleaseInfo()})
	{
		cover = getFromDirectory(releaseInfo->releaseDirectory, width);
		if (!cover)
			cover = getFromTrack(session, releaseInfo->firstTrackId, width, false /* no release fallback */);
	}

	if (!cover)
		cover = getDefault(width);

	if (cover)
		saveToCache(cacheEntryDesc, cover);

	return cover;
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

void
Grabber::saveToCache(const CacheEntryDesc& entryDesc, std::shared_ptr<IEncodedImage> image)
{
	std::unique_lock lock {_cacheMutex};

	while (_cacheSize + image->getDataSize() > _maxCacheSize && !_cache.empty())
	{
		auto itRandom {Random::pickRandom(_cache)};
		_cacheSize -= itRandom->second->getDataSize();
		_cache.erase(itRandom);
	}

	_cacheSize += image->getDataSize();
	_cache[entryDesc] = image;
}

std::shared_ptr<IEncodedImage>
Grabber::loadFromCache(const CacheEntryDesc& entryDesc)
{
	std::shared_lock lock {_cacheMutex};

	auto it {_cache.find(entryDesc)};
	if (it == std::cend(_cache))
	{
		++_cacheMisses;
		return nullptr;
	}

	++_cacheHits;
	return it->second;
}

} // namespace CoverArt

