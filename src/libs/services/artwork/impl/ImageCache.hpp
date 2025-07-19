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
#include <cassert>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <unordered_map>

#include "database/objects/ArtworkId.hpp"
#include "image/IEncodedImage.hpp"

namespace lms::artwork
{
    class ImageCache
    {
    public:
        ImageCache(std::size_t maxCacheSize);

        struct EntryDesc
        {
            db::ArtworkId id;
            std::optional<std::size_t> size;

            bool operator==(const EntryDesc& other) const = default;
        };

        std::size_t getMaxCacheSize() const { return _maxCacheSize; }

        void addImage(const EntryDesc& entryDesc, std::shared_ptr<image::IEncodedImage> image);
        std::shared_ptr<image::IEncodedImage> getImage(const EntryDesc& entryDesc) const;
        void flush();

    private:
        const std::size_t _maxCacheSize;

        mutable std::shared_mutex _mutex;

        struct EntryHasher
        {
            std::size_t operator()(const EntryDesc& entry) const
            {
                assert(entry.size); // should not cache unresized images
                return std::hash<db::ArtworkId>{}(entry.id) ^ std::hash<std::size_t>{}(*entry.size);
            }
        };

        std::unordered_map<EntryDesc, std::shared_ptr<image::IEncodedImage>, EntryHasher> _cache;
        std::size_t _cacheSize{};
        mutable std::atomic<std::size_t> _cacheMisses;
        mutable std::atomic<std::size_t> _cacheHits;
    };
} // namespace lms::artwork