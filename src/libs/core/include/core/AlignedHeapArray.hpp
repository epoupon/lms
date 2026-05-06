/*
 * Copyright (C) 2026 Emeric Poupon
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

#include <cstddef>
#include <cstdlib>

#include "core/Exception.hpp"

namespace lms::core
{
    template<typename T, std::size_t Alignment>
    class AlignedHeapArray
    {
    public:
        using iterator = T*;
        using const_iterator = const T*;

        explicit AlignedHeapArray(std::size_t count)
            : _count{ count }
            , _values{ static_cast<T*>(std::aligned_alloc(Alignment, getStorageSize(count))) }
        {
            if (!_values)
                throw LmsException{ "Allocation failed" };
        }

        ~AlignedHeapArray()
        {
            std::free(_values);
        }

        AlignedHeapArray(const AlignedHeapArray&) = delete;
        AlignedHeapArray& operator=(const AlignedHeapArray&) = delete;

        std::size_t size() const { return _count; }
        T* data() const { return _values; }
        T& operator[](std::size_t index) const { return _values[index]; }

        iterator begin() const { return _values; }
        iterator end() const { return _values + _count; }

        const_iterator cbegin() const { return _values; }
        const_iterator cend() const { return _values + _count; }

    private:
        static std::size_t getStorageSize(std::size_t count)
        {
            const std::size_t bytes{ count * sizeof(T) };
            const std::size_t alignedBytes{ ((bytes + Alignment - 1) / Alignment) * Alignment };
            return alignedBytes;
        }

        const std::size_t _count;
        T* _values;
    };
} // namespace lms::core