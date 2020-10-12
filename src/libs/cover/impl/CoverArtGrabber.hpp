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
#include <optional>
#include <shared_mutex>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "cover/ICoverArtGrabber.hpp"
#include "database/Types.hpp"
#include "Image.hpp"

namespace Database
{
	class Session;
}

namespace CoverArt
{
	struct CacheEntryDesc
	{
		enum class Type
		{
			Track,
			Release,
		};

		Type				type;
		Database::IdType	id;
		std::size_t			size;

		bool operator==(const CacheEntryDesc& other) const
		{
			return type == other.type
				&& id == other.id
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
				size_t h = std::hash<int>()(static_cast<int>(e.type));
				h ^= std::hash<Database::IdType>()(e.id) << 1;
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
			Grabber(const std::filesystem::path& execPath, std::size_t maxCacheEntries, std::size_t maxFileSize);

			Grabber(const Grabber&) = delete;
			Grabber& operator=(const Grabber&) = delete;
			Grabber(Grabber&&) = delete;
			Grabber& operator=(Grabber&&) = delete;

		private:

			void							setDefaultCover(const std::filesystem::path& defaultCoverPath) override;
			std::unique_ptr<ICoverArt>		getFromTrack(Database::Session& dbSession, Database::IdType trackId, Width width) override;
			std::unique_ptr<ICoverArt>		getFromRelease(Database::Session& dbSession, Database::IdType releaseId, Width width) override;
			void							flushCache() override;

			EncodedImage					getFromTrackInternal(Database::Session& dbSession, Database::IdType trackId, Width width);
			EncodedImage					getFromReleaseInternal(Database::Session& dbSession, Database::IdType releaseId, Width width);

			std::optional<EncodedImage>		getFromTrack(const std::filesystem::path& path, Width width) const;
			std::multimap<std::string, std::filesystem::path>	getCoverPaths(const std::filesystem::path& directoryPath) const;
			std::optional<EncodedImage>		getFromDirectory(const std::filesystem::path& path, std::string_view preferredFileName, Width width) const;
			EncodedImage					getDefault(Width width);

			EncodedImage					resizeCoverOrFallback(EncodedImage image, Width width) const;

			std::optional<EncodedImage>		_defaultCover;	// optional to defer initializing

			std::shared_mutex _cacheMutex;
			std::unordered_map<CacheEntryDesc, EncodedImage> _cache;
			std::unordered_map<Width, EncodedImage>	_defaultCache;
			std::atomic<std::size_t>	_cacheMisses {};
			std::atomic<std::size_t>	_cacheHits {};
			std::size_t					_cacheSize {};

			void saveToCache(const CacheEntryDesc& entryDesc, const EncodedImage& image);
			std::optional<EncodedImage> loadFromCache(const CacheEntryDesc& entryDesc);

			const std::size_t _maxCacheSize;
			static inline const std::vector<std::filesystem::path> _fileExtensions {".jpg", ".jpeg", ".png", ".bmp"}; // TODO parametrize
			const std::size_t _maxFileSize;
			static inline const std::vector<std::string> _preferredFileNames {"cover", "front"}; // TODO parametrize
	};

} // namespace CoverArt

