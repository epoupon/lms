/*
 * Copyright (C) 2015 Emeric Poupon
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

#pragma once

#include <atomic>
#include <filesystem>
#include <map>
#include <optional>
#include <shared_mutex>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "cover/ICoverArtGrabber.hpp"
#include "cover/IEncodedImage.hpp"
#include "database/Types.hpp"

namespace Database
{
	class Session;
}

namespace Av
{
	class IAudioFile;
}

namespace CoverArt
{
	struct CacheEntryDesc
	{
		std::variant<Database::TrackId, Database::ReleaseId> id;
		std::size_t			size;

		bool operator==(const CacheEntryDesc& other) const
		{
			return id == other.id
				&& size == other.size;
		}
	};

} // ns CoverArt

namespace std
{

	template<>
	class hash<CoverArt::CacheEntryDesc>
	{
		public:
			size_t operator()(const CoverArt::CacheEntryDesc& e) const
			{
				size_t h {};
				std::visit([&](auto id)
				{
					using IdType = std::decay_t<decltype(id)>;
					h ^= std::hash<IdType>()(id);
				}, e.id);
				h ^= std::hash<std::size_t>()(e.size) << 1;
				return h;
			}
	};

} // ns std

namespace CoverArt
{
	class Grabber : public IGrabber
	{
		public:
			Grabber(Database::Db& db,
					const std::filesystem::path& execPath,
					const std::filesystem::path& defaultCoverPath);

			Grabber(const Grabber&) = delete;
			Grabber& operator=(const Grabber&) = delete;
			Grabber(Grabber&&) = delete;
			Grabber& operator=(Grabber&&) = delete;

		private:
			std::shared_ptr<IEncodedImage>	getFromTrack(Database::TrackId trackId, ImageSize width) override;
			std::shared_ptr<IEncodedImage>	getFromRelease(Database::ReleaseId releaseId, ImageSize width) override;
			void							flushCache() override;
			void							setJpegQuality(unsigned quality) override;

			std::shared_ptr<IEncodedImage>	getFromTrack(Database::Session& dbSession, Database::TrackId trackId, ImageSize width, bool allowReleaseFallback);
			std::unique_ptr<IEncodedImage>	getFromAvMediaFile(const Av::IAudioFile& input, ImageSize width) const;
			std::unique_ptr<IEncodedImage>	getFromCoverFile(const std::filesystem::path& p, ImageSize width) const;

			std::unique_ptr<IEncodedImage>	getFromTrack(const std::filesystem::path& path, ImageSize width) const;
			std::multimap<std::string, std::filesystem::path>	getCoverPaths(const std::filesystem::path& directoryPath) const;
			std::unique_ptr<IEncodedImage>	getFromDirectory(const std::filesystem::path& directory, ImageSize width) const;
			std::unique_ptr<IEncodedImage>	getFromSameNamedFile(const std::filesystem::path& filePath, ImageSize width) const;
			std::shared_ptr<IEncodedImage>	getDefault(ImageSize width);

			bool							checkCoverFile(const std::filesystem::path& directoryPath) const;

			Database::Db&				_db;

			std::shared_mutex _cacheMutex;
			std::unordered_map<CacheEntryDesc, std::shared_ptr<IEncodedImage>> _cache;
			std::unordered_map<ImageSize, std::shared_ptr<IEncodedImage>> _defaultCoverCache;
			std::atomic<std::size_t>	_cacheMisses {};
			std::atomic<std::size_t>	_cacheHits {};
			std::size_t					_cacheSize {};

			void saveToCache(const CacheEntryDesc& entryDesc, std::shared_ptr<IEncodedImage> image);
			std::shared_ptr<IEncodedImage> loadFromCache(const CacheEntryDesc& entryDesc);

			const std::filesystem::path _defaultCoverPath;
			const std::size_t _maxCacheSize;
			static inline const std::vector<std::filesystem::path> _fileExtensions {".jpg", ".jpeg", ".png", ".bmp"}; // TODO parametrize
			const std::size_t _maxFileSize;
			static inline const std::vector<std::string> _preferredFileNames {"cover", "front"}; // TODO parametrize
			unsigned _jpegQuality;
	};

} // namespace CoverArt

