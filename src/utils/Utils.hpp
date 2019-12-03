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

#include <chrono>
#include <list>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <sstream>
#include <vector>

#include <Wt/WDate.h>

bool
readList(const std::string& str, const std::string& separators, std::list<std::string>& results);

std::vector<std::string>
splitString(const std::string& string, const std::string& separators);

std::string
joinStrings(const std::vector<std::string>& strings, const std::string& delimiter);

std::string
stringTrim(const std::string& str, const std::string& whitespaces = " \t");

std::string
stringTrimEnd(const std::string& str, const std::string& whitespaces = " \t");

std::string
stringToLower(const std::string& str);

std::string
bufferToString(const std::vector<unsigned char>& data);

template<typename T>
std::optional<T> readAs(const std::string& str)
{
	T res;

	std::istringstream iss ( str );
	iss >> res;
	if (iss.fail())
		return std::nullopt;

	return res;
}

std::string
replaceInString(const std::string& str, const std::string& from, const std::string& to);

bool
stringEndsWith(const std::string& str, const std::string& ending);

std::optional<std::string>
stringFromHex(const std::string& str);

// warning: not efficient
template<class In, class Out, class U = typename std::iterator_traits<In>::value_type>
void uniqueAndSortedByOccurence(In first, In last, Out out)
{
	std::map<U, std::size_t> occurencesMap;

	for (In it = first; it != last; ++it)
	{
		if (occurencesMap.find(*it) == occurencesMap.end())
			occurencesMap[*it] = 0;

		occurencesMap[*it]++;
	}

	struct Item
	{
		U elem;
		std::size_t count;
	};

	std::vector<Item> occurencesVector;
	for (const auto& occurence : occurencesMap)
		occurencesVector.emplace_back(Item{occurence.first, occurence.second});

	std::sort(occurencesVector.begin(), occurencesVector.end(), [](const auto& a, const auto& b) { return a.count > b.count;});

	for (const auto& occurence : occurencesVector)
		*out++ = occurence.elem;
}

template<class T, class Compare = std::less<>>
constexpr T clamp(T v, T lo, T hi, Compare comp = {})
{
	assert(!comp(hi, lo));
	return comp(v, lo) ? lo : comp(hi, v) ? hi : v;
}

using RandGenerator = std::mt19937;
RandGenerator& getRandGenerator();

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


