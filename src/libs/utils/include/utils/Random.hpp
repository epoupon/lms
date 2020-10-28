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

#include <algorithm>
#include <random>

namespace Random {

using RandGenerator = std::mt19937;
RandGenerator& getRandGenerator();

RandGenerator createSeededGenerator(uint_fast32_t seed);

template <typename T>
T
getRandom(T min, T max)
{
	std::uniform_int_distribution<> dist {min, max};
	return dist (getRandGenerator());
}

template <typename T>
T
getRealRandom(T min, T max)
{
	std::uniform_real_distribution<> dist {min, max};
	return dist (getRandGenerator());
}

template <typename Container>
void
shuffleContainer(Container& container)
{
	std::shuffle(std::begin(container), std::end(container), getRandGenerator());
}

template <typename Container>
typename Container::const_iterator
pickRandom(const Container& container)
{
	if (container.empty())
		return std::end(container);

	return std::next(std::begin(container), getRandom(0, static_cast<int>(container.size() - 1)));
}

}

