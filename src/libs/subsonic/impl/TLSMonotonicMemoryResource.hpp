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

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

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
            allocateNewBlock(defaultBlockSize);
        }

        [[nodiscard]] std::byte* allocate(std::size_t byteCount, std::size_t alignment)
        {
            if (byteCount == 0)
                byteCount = 1;

            std::byte* currentAddrAligned{ computeAlignedAddr(_currentAddr, alignment) };

            if (!_currentBlock->fitsInBlock(currentAddrAligned, byteCount))
            {
                allocateNewBlock(std::max(defaultBlockSize, byteCount + alignment /* worst case */));
                currentAddrAligned = computeAlignedAddr(_currentAddr, alignment);
            }
            assert(_currentBlock->fitsInBlock(currentAddrAligned, byteCount));

            std::byte* res{ currentAddrAligned };
            _currentAddr = currentAddrAligned + byteCount;

            return res;
        }

        void deallocate(std::byte* /* ptr */)
        {
            // nothing to do!
        }

        void reset()
        {
            // First: remove blocks that are not default-sized
            std::erase_if(_blocks, [](const Block& _block) { return _block.size != defaultBlockSize; });

            // Then always keep at least one block
            if (_blocks.size() > 1)
                _blocks.erase(std::next(std::cbegin(_blocks), 1), std::cend(_blocks));

            _currentBlock = &_blocks.front();
            _currentAddr = _currentBlock->data.get();
        }

    private:
        void allocateNewBlock(std::size_t size)
        {
            Block block{
                .data = std::make_unique_for_overwrite<std::byte[]>(size),
                .size = size,
            };

            _currentAddr = block.data.get();
            _currentBlock = &_blocks.emplace_back(std::move(block));
        }

        static std::byte* computeAlignedAddr(std::byte* addr, std::size_t alignment) noexcept
        {
            const std::uintptr_t addrValue{ reinterpret_cast<std::uintptr_t>(addr) };

            const std::uintptr_t mask{ alignment - 1 };
            const std::uintptr_t adjustment{ (alignment - (addrValue & mask)) & mask };
            return reinterpret_cast<std::byte*>(addr + adjustment);
        }

        static constexpr std::size_t defaultBlockSize{ static_cast<std::size_t>(1 * 1024 * 1024) };
        struct Block
        {
            std::unique_ptr<std::byte[]> data;
            std::size_t size;

            bool fitsInBlock(std::byte* addr, std::size_t rangeSize) const
            {
                return data && rangeSize && addr >= data.get() && addr + rangeSize <= data.get() + size;
            }
        };

        std::vector<Block> _blocks;
        Block* _currentBlock{};
        std::byte* _currentAddr{};
    };
} // namespace lms::api::subsonic