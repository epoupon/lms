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

#include "ImageCache.hpp"

#include <mutex>

#include "core/ILogger.hpp"
#include "core/Random.hpp"

namespace lms::artwork
{
    ImageCache::ImageCache(std::size_t maxCacheSize)
        : _maxCacheSize{ maxCacheSize }
    {
    }

    void ImageCache::addImage(const EntryDesc& entryDesc, std::shared_ptr<image::IEncodedImage> image)
    {
        // cache only resized files
        if (!entryDesc.size)
            return;

        const std::unique_lock lock{ _mutex };

        while (_cacheSize + image->getData().size() > _maxCacheSize && !_cache.empty())
        {
            auto itRandom{ core::random::pickRandom(_cache) };
            _cacheSize -= itRandom->second->getData().size();
            _cache.erase(itRandom);
        }

        _cacheSize += image->getData().size();
        _cache[entryDesc] = image;
    }

    std::shared_ptr<image::IEncodedImage> ImageCache::getImage(const EntryDesc& entryDesc) const
    {
        // cache only resized files
        if (!entryDesc.size)
            return {};

        const std::shared_lock lock{ _mutex };

        const auto it{ _cache.find(entryDesc) };
        if (it == std::cend(_cache))
        {
            ++_cacheMisses;
            return nullptr;
        }

        ++_cacheHits;
        return it->second;
    }

    void ImageCache::flush()
    {
        const std::unique_lock lock{ _mutex };

        LMS_LOG(COVER, DEBUG, "Cache stats: hits = " << _cacheHits.load() << ", misses = " << _cacheMisses.load() << ", nb entries = " << _cache.size() << ", size = " << _cacheSize);
        _cacheHits = 0;
        _cacheMisses = 0;
        _cacheSize = 0;
        _cache.clear();
    }
} // namespace lms::artwork