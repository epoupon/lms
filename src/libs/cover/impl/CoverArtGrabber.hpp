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

#include <filesystem>
#include <shared_mutex>
#include <optional>
#include <vector>
#include <unordered_map>

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
			Grabber(const std::filesystem::path& execPath);

			Grabber(const Grabber&) = delete;
			Grabber& operator=(const Grabber&) = delete;
			Grabber(Grabber&&) = delete;
			Grabber& operator=(Grabber&&) = delete;

		private:

			void			setDefaultCover(const std::filesystem::path& defaultCoverPath) override;

			std::vector<uint8_t>	getFromTrack(Database::Session& dbSession, Database::IdType trackId, Format format, std::size_t width) override;
			std::vector<uint8_t>	getFromRelease(Database::Session& dbSession, Database::IdType releaseId, Format format, std::size_t width) override;
			void			flushCache() override;


			Image					getFromTrack(Database::Session& dbSession, Database::IdType trackId, std::size_t size);
			Image					getFromRelease(Database::Session& dbSession, Database::IdType releaseId, std::size_t size);

			std::optional<Image>			getFromTrack(const std::filesystem::path& path) const;
			std::vector<std::filesystem::path>	getCoverPaths(const std::filesystem::path& directoryPath) const;
			std::optional<Image>			getFromDirectory(const std::filesystem::path& path) const;

			std::unique_ptr<Image> _defaultCover;	// unique_ptr to defer initializing

			std::shared_mutex _cacheMutex;
			std::unordered_map<CacheEntryDesc, Image> _cache;
			std::size_t _cacheMisses {};
			std::size_t _cacheHits {};

			void saveToCache(const CacheEntryDesc& entryDesc, const Image& image);
			std::optional<Image> loadFromCache(const CacheEntryDesc& entryDesc);

			static inline constexpr std::size_t _maxCacheEntries {1000};
			static inline const std::vector<std::filesystem::path> _fileExtensions {".jpg", ".jpeg", ".png", ".bmp"}; // TODO parametrize
			static inline constexpr std::size_t _maxFileSize {10000000};
			static inline const std::vector<std::filesystem::path> _preferredFileNames {"cover", "front"}; // TODO parametrize
	};

} // namespace CoverArt

