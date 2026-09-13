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
#include <type_traits>

namespace lms::core::random
{
    using PseudoRandomGenerator = std::mt19937;
    PseudoRandomGenerator& getPseudoRandomGenerator();

    using NonDeterministicRandomGenerator = std::random_device;
    NonDeterministicRandomGenerator& getNonDeterministicRandomGenerator();

    template<typename Generator, typename T>
        requires std::is_integral_v<T>
    T generate(Generator& generator, T min, T max)
    {
        std::uniform_int_distribution<T> dist{ min, max };
        return dist(generator);
    }

    template<typename Generator, typename T>
        requires std::is_floating_point_v<T>
    T generate(Generator& generator, T min, T max)
    {
        std::uniform_real_distribution<T> dist{ min, max };
        return dist(generator);
    }

    template<typename T>
    T generate(T min, T max)
    {
        return generate(getPseudoRandomGenerator(), min, max);
    }

    template<typename RandomEngine, typename Container>
        requires std::is_floating_point_v<typename Container::value_type>
    void fillContainer(RandomEngine& randomEngine, Container& container, typename Container::value_type min, typename Container::value_type max)
    {
        std::uniform_real_distribution<typename Container::value_type> distrib{ min, max };
        for (auto& v : container)
            v = distrib(randomEngine);
    }

    template<typename RandomEngine, typename Container>
        requires std::is_integral_v<typename Container::value_type>
    void fillContainer(RandomEngine& randomEngine, Container& container, typename Container::value_type min, typename Container::value_type max)
    {
        std::uniform_int_distribution<typename Container::value_type> distrib{ min, max };
        for (auto& v : container)
            v = distrib(randomEngine);
    }

    template<typename Container>
    void shuffleContainer(Container& container)
    {
        std::shuffle(std::begin(container), std::end(container), getPseudoRandomGenerator());
    }

    template<typename RandomEngine, typename Container>
    void shuffleContainer(RandomEngine& randomEngine, Container& container)
    {
        std::shuffle(std::begin(container), std::end(container), randomEngine);
    }

    template<typename Container>
    typename Container::const_iterator pickRandom(const Container& container)
    {
        if (container.empty())
            return std::end(container);

        return std::next(std::begin(container), generate(0, static_cast<int>(container.size() - 1)));
    }
} // namespace lms::core::random