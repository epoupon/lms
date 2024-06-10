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

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <list>

namespace lms::api::subsonic
{
    class TLSMonotonicMemoryResource
    {
    public:
        static TLSMonotonicMemoryResource& getInstance()
        {
            static thread_local TLSMonotonicMemoryResource instance;
            return instance;
        }

        TLSMonotonicMemoryResource()
        {
            allocateNewBlock();
        }

        [[nodiscard]] std::byte* allocate(std::size_t byteCount, std::size_t alignment)
        {
            std::byte* currentAddrAligned{ computeAlignedAddr(_currentAddr, alignment) };

            if (currentAddrAligned + byteCount > &_currentBlock->back() + 1)
            {
                allocateNewBlock();
                currentAddrAligned = computeAlignedAddr(_currentAddr, alignment);
            }

            // Requested too many bytes for blockSize!
            if (currentAddrAligned + byteCount > &_currentBlock->back() + 1)
                throw std::bad_alloc{};

            assert(currentAddrAligned >= &_currentBlock->front());

            std::byte* res{ currentAddrAligned };
            _currentAddr = currentAddrAligned + byteCount;

            return res;
        }

        void deallocate(std::byte*)
        {
            // nothing to do!
        }

        void reset()
        {
            // always keep at least one block
            if (_blocks.size() > 1)
                _blocks.erase(std::next(std::cbegin(_blocks), 1), std::cend(_blocks));

            _currentBlock = &_blocks.front();
            _currentAddr = _currentBlock->data();
        }

    private:
        void allocateNewBlock()
        {
            _currentBlock = &_blocks.emplace_back();
            _currentAddr = _currentBlock->data();
        }

        static std::byte* computeAlignedAddr(std::byte* addr, std::size_t alignment) noexcept
        {
            const std::uintptr_t addrValue{ reinterpret_cast<std::uintptr_t>(addr) };

            const std::uintptr_t mask{ alignment - 1 };
            const std::uintptr_t adjustment{ (alignment - (addrValue & mask)) & mask };
            return reinterpret_cast<std::byte*>(addr + adjustment);
        }

        static constexpr std::size_t blockSize{ static_cast<std::size_t>(1 * 1024 * 1024) };
        using BlockType = std::array<std::byte, blockSize>;

        std::list<BlockType> _blocks;
        BlockType* _currentBlock{};
        std::byte* _currentAddr{};
    };
} // namespace lms::api::subsonic