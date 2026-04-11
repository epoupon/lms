/*
 * Copyright (C) 2018 Emeric Poupon
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
#include <random>

namespace lms::som
{
    namespace detail
    {
        template<typename FloatType>
        FloatType computeSquaredDist(const MatrixPosition& a, const MatrixPosition& b)
        {
            const FloatType dx{ static_cast<FloatType>(b.x) - static_cast<FloatType>(a.x) };
            const FloatType dy{ static_cast<FloatType>(b.y) - static_cast<FloatType>(a.y) };
            return dx * dx + dy * dy;
        }

        template<typename FloatType>
        FloatType computeLearningRate(std::size_t epoch, std::size_t epochCount, FloatType initialLearningRate)
        {
            const FloatType t{ static_cast<FloatType>(epoch) / static_cast<FloatType>(epochCount) };
            return initialLearningRate * std::exp(-t);
        }

        template<typename FloatType>
        FloatType computeNeighborhoodRadius(std::size_t epoch, std::size_t epochCount, FloatType initialRadius)
        {
            const FloatType t{ static_cast<FloatType>(epoch) / static_cast<FloatType>(epochCount) };
            return initialRadius * std::exp(-t);
        }

        template<typename FloatType>
        void computeInfluenceLUT(std::vector<FloatType>& influenceLUT, FloatType sigma)
        {
            const FloatType squaredSigma{ sigma * sigma };
            const std::size_t maxSquaredDist{ static_cast<std::size_t>(std::ceil(3 * squaredSigma)) };
            const FloatType invTwoSquaredSigma{ FloatType{ 1 } / (FloatType{ 2 } * squaredSigma) };

            influenceLUT.resize(maxSquaredDist + 1);

            for (std::size_t squaredDist{}; squaredDist <= maxSquaredDist; ++squaredDist)
                influenceLUT[squaredDist] = std::exp(-static_cast<FloatType>(squaredDist) * invTwoSquaredSigma);
        }
    } // namespace detail

    template<std::size_t DimensionCount, typename FloatType>
    Network<DimensionCount, FloatType>::Network(Coordinate width, Coordinate height)
        : _neurons{ width, height }
        , _initialRadius{ std::max(width, height) / FloatType{ 2 } }
        , _initialLearningRate{ 0.1F } // // should be good enough for normalized data
        , _weights{ 1.F }
    {
    }

    template<std::size_t DimensionCount, typename FloatType>
    template<typename RandomEngine>
    Network<DimensionCount, FloatType>::Network(Coordinate width, Coordinate height, RandomEngine& randomEngine, FloatType min, FloatType max)
        : Network{ width, height }
    {
        std::uniform_real_distribution<FloatType> distrib{ min, max };

        for (Coordinate y{}; y < height; ++y)
        {
            for (Coordinate x{}; x < width; ++x)
            {
                for (auto& v : _neurons.get({ x, y }))
                    v = distrib(randomEngine);
            }
        }
    }

    template<std::size_t DimensionCount, typename FloatType>
    void Network<DimensionCount, FloatType>::setNeuron(const MatrixPosition& position, const Vector& neuron)
    {
        _neurons.get(position) = neuron;
    }

    template<std::size_t DimensionCount, typename FloatType>
    const typename Network<DimensionCount, FloatType>::Vector& Network<DimensionCount, FloatType>::getNeuron(const MatrixPosition& position) const
    {
        return _neurons.get(position);
    }

    template<std::size_t DimensionCount, typename FloatType>
    MatrixPosition Network<DimensionCount, FloatType>::getBestMatchingNeuron(const Vector& input) const
    {
        return _neurons.getPositionMinDistance(SquaredEuclideanDistanceWithWeights{ input, _weights });
    }

    template<std::size_t DimensionCount, typename FloatType>
    void Network<DimensionCount, FloatType>::beginTraining(std::size_t epochCount)
    {
        assert(epochCount > 0);
        _epochCount = epochCount;
        _epoch = 0;
    }

    template<std::size_t DimensionCount, typename FloatType>
    void Network<DimensionCount, FloatType>::beginNextEpoch()
    {
        assert(_epoch < _epochCount);

        {
            constexpr FloatType lrFinal{ 0.01 };
            _learningRate = _initialLearningRate * std::pow(lrFinal / _initialLearningRate, static_cast<FloatType>(_epoch) / static_cast<FloatType>(_epochCount));
        }
        {
            constexpr FloatType sigmaFinal{ 1 };
            _sigma = _initialRadius * std::pow(sigmaFinal / _initialRadius, static_cast<FloatType>(_epoch) / static_cast<FloatType>(_epochCount));
        }

        detail::computeInfluenceLUT(_influenceLUT, _sigma);

        _epoch += 1;
    }

    template<std::size_t DimensionCount, typename FloatType>
    void Network<DimensionCount, FloatType>::train(const Vector& input)
    {
        const MatrixPosition bestMatchingNeuronPos{ getBestMatchingNeuron(input) };
        for (Coordinate y{}; y < _neurons.getHeight(); ++y)
        {
            for (Coordinate x{}; x < _neurons.getWidth(); ++x)
            {
                const MatrixPosition neuronPos{ x, y };
                const FloatType squaredGridDist{ detail::computeSquaredDist<FloatType>(neuronPos, bestMatchingNeuronPos) };

                if (squaredGridDist >= _influenceLUT.size())
                    continue;

                const FloatType influence{ _influenceLUT[static_cast<std::size_t>(squaredGridDist)] };

                Vector& neuron{ _neurons.get(neuronPos) };
                for (std::size_t i{}; i < DimensionCount; ++i)
                    neuron[i] += _learningRate * influence * (input[i] - neuron[i]);
            }
        }
    }
} // namespace lms::som
