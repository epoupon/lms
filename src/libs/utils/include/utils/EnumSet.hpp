/*
 * Copyright (C) 2020 Emeric Poupon
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
#include <type_traits>

template <typename T, typename underlying_type = std::uint32_t>
class EnumSet
{
	static_assert(std::is_enum<T>::value);
	static_assert(std::is_same<underlying_type, std::uint64_t>::value || std::is_same<underlying_type, std::uint32_t>::value);

	using index_type = std::uint_fast8_t;

	public:
		EnumSet() = default;
		constexpr EnumSet(std::initializer_list<T> values)
		{
			for (T value : values)
				insert(value);
		}

		template <typename It>
		constexpr EnumSet(It begin, It end)
		{
			for (It it {begin}; it != end; ++it)
				insert(*it);
		}

		constexpr void insert(T value)
		{
			assert(static_cast<size_t>(value) < sizeof(_bitfield) * 8);
			_bitfield |= (underlying_type{ 1 } << static_cast<underlying_type>(value));
		}

		constexpr void erase(T value)
		{
			assert(static_cast<size_t>(value) < sizeof(_bitfield) * 8);
			_bitfield &= ~(underlying_type{ 1 } << static_cast<underlying_type>(value));
		}

		constexpr bool empty() const
		{
			return _bitfield == 0;
		}

		constexpr bool contains(T value) const
		{
			assert(static_cast<size_t>(value) < sizeof(_bitfield) * 8);
			return _bitfield & (underlying_type{ 1 } << static_cast<underlying_type>(value));
		}

		class iterator
		{
			public:
				using value_type = T;

				constexpr value_type operator*() const
				{
					return static_cast<value_type>(_index);
				}

				constexpr bool operator==(const iterator& _other) const
				{
					return &_container == &_other._container && _index == _other._index;
				}

				constexpr bool operator!=(const iterator& _other) const
				{
					return !(*this == _other);
				}

				constexpr iterator& operator++()
				{
					_index = _container.getFirstBitSetIndex(_index + 1);
					return *this;
				}

			private:
				friend class EnumSet;

				constexpr iterator(const EnumSet& _container, index_type _index)
					: _container {_container}
					, _index {_index}
				{
				}

				const EnumSet& _container;
				index_type _index;
		};

		constexpr iterator begin() const
		{
			return iterator {*this, getFirstBitSetIndex()};
		}

		constexpr iterator end() const
		{
			return iterator {*this, npos};
		}

	private:
		static_assert(std::numeric_limits<index_type>::max() >= sizeof(underlying_type) * 8);
		enum : index_type { npos = sizeof(underlying_type) * 8 };

		constexpr index_type getFirstBitSetIndex(index_type start = {}) const
		{
			assert(start < npos);

			// return npos if no bit found
			index_type res {countTrailingZero(_bitfield >> start)};
			if (res == npos)
				return res;

			return res + start;
		}

		static constexpr index_type countTrailingZero(underlying_type bitField)
		{
			index_type res {};

			while (res < (sizeof(underlying_type) * 8) && (bitField & 1) == 0)
			{
				++res;
				bitField >>= 1;
			}

			if (res == sizeof(underlying_type) * 8)
				res = npos;

			return res;
		}

		underlying_type _bitfield{};
};


