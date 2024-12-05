/*
 * Copyright (C) 2024 Emeric Poupon
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

#include "TLSMonotonicMemoryResource.hpp"

namespace lms::api::subsonic
{
    // Stateless allocator that uses a shared MemoryResource
    template<typename MemoryResource, typename T>
    class Allocator
    {
    public:
        using value_type = T;
        using pointer = T*;
        using const_pointer = const T*;
        using reference = T&;
        using const_reference = const T&;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;

        constexpr Allocator() noexcept = default;

        template<typename U>
        constexpr Allocator(const Allocator<MemoryResource, U>& /*allocator*/) noexcept
        {
        }

        template<typename V>
        struct rebind
        {
            using other = Allocator<MemoryResource, V>;
        };

        [[nodiscard]] pointer allocate(size_type n)
        {
            return reinterpret_cast<pointer>(MemoryResource::getInstance().allocate(n * sizeof(T), alignof(T)));
        }

        // Deallocate memory pointed to by p
        void deallocate(pointer p, std::size_t /*n*/) noexcept
        {
            MemoryResource::getInstance().deallocate(reinterpret_cast<std::byte*>(p));
        }
    };

    template<class MemoryResource, class T, class U>
    bool operator==(const Allocator<MemoryResource, T>&, const Allocator<MemoryResource, U>&)
    {
        return true;
    }

    template<class MemoryResource, class T, class U>
    bool operator!=(const Allocator<MemoryResource, T>&, const Allocator<MemoryResource, U>&)
    {
        return false;
    }

    template<typename T>
    using ResponseAllocator = Allocator<TLSMonotonicMemoryResource, T>;
} // namespace lms::api::subsonic