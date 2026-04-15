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

#include <algorithm>
#include <cmath>
#include <vector>

#include "Matrix.hpp"

namespace lms::som
{
    struct TrainerParams
    {
        std::size_t epochCount;
    };

    template<typename Network>
    class Trainer
    {
    public:
        using Vector = typename Network::Vector;
        using FloatType = typename Network::Vector::value_type;

        // User is responsible to provide a properly initialized network
        Trainer(Network& network, const TrainerParams& params);
        ~Trainer() = default;
        Trainer(const Trainer&) = delete;
        Trainer& operator=(const Trainer&) = delete;

        void beginEpoch();
        void train(const Vector& inputVector);

        FloatType getLearningRate() const { return _learningRate; }
        FloatType getSigma() const { return _sigma; }

    private:
        const FloatType _initialRadius;
        const FloatType _initialLearningRate;
        const std::size_t _epochCount;
        Network& _network;
        std::size_t _epoch{};
        FloatType _learningRate{};
        FloatType _sigma{};
        int _influenceRadius{};
        std::vector<FloatType> _influenceLUT;
        std::vector<int> _xRadiusByAbsDy;
    };

    template<typename Network>
    Trainer<Network>::Trainer(Network& network, const TrainerParams& params)
        : _initialRadius{ std::max(network.getWidth(), network.getHeight()) / FloatType{ 2 } }
        , _initialLearningRate{ 0.1F } // // should be good enough for normalized data
        , _epochCount{ params.epochCount }
        , _network{ network }
        , _epoch{}
    {
    }

    template<typename Network>
    void Trainer<Network>::beginEpoch()
    {
        assert(_epoch < _epochCount);

        {
            constexpr FloatType learningRateFinal{ 0.01 };
            _learningRate = _initialLearningRate * std::pow(learningRateFinal / _initialLearningRate, static_cast<FloatType>(_epoch) / static_cast<FloatType>(_epochCount));
        }
        {
            constexpr FloatType sigmaFinal{ 1 };
            _sigma = _initialRadius * std::pow(sigmaFinal / _initialRadius, static_cast<FloatType>(_epoch) / static_cast<FloatType>(_epochCount));
        }

        {
            const FloatType maxDist{ 3 * _sigma };
            const std::size_t maxSquaredDist{ static_cast<std::size_t>(std::ceil(maxDist * maxDist)) };
            const FloatType invTwoSquaredSigma{ FloatType{ 1 } / (FloatType{ 2 } * _sigma * _sigma) };

            _influenceLUT.resize(maxSquaredDist + 1);
            for (std::size_t squaredDist{}; squaredDist <= maxSquaredDist; ++squaredDist)
                _influenceLUT[squaredDist] = std::exp(-static_cast<FloatType>(squaredDist) * invTwoSquaredSigma);

            _influenceRadius = std::floor(std::sqrt(static_cast<FloatType>(maxSquaredDist)));
            _xRadiusByAbsDy.resize(_influenceRadius + 1);
            for (std::size_t absDy{}; absDy <= static_cast<std::size_t>(_influenceRadius); ++absDy)
            {
                const std::size_t squaredDy{ absDy * absDy };
                const std::size_t maxSquaredDx{ maxSquaredDist - squaredDy };
                _xRadiusByAbsDy[absDy] = static_cast<int>(std::floor(std::sqrt(static_cast<FloatType>(maxSquaredDx))));
            }
        }

        _epoch += 1;
    }

    template<typename Network>
    void Trainer<Network>::train(const Vector& input)
    {
        const MatrixPosition bestMatchingNeuronPos{ _network.getBestMatchingNeuron(input) };

        const int yMin{ std::max(0, static_cast<int>(bestMatchingNeuronPos.y) - _influenceRadius) };
        const int yMax{ std::min(static_cast<int>(_network.getHeight()) - 1, static_cast<int>(bestMatchingNeuronPos.y) + _influenceRadius) };

        for (int y{ yMin }; y <= yMax; ++y)
        {
            const int dy{ y - static_cast<int>(bestMatchingNeuronPos.y) };
            const std::size_t squaredDy{ static_cast<std::size_t>(dy * dy) };
            const std::size_t absDy{ static_cast<std::size_t>(std::abs(dy)) };
            const int xRadius{ _xRadiusByAbsDy[absDy] };

            const int xMin{ std::max(0, static_cast<int>(bestMatchingNeuronPos.x) - xRadius) };
            const int xMax{ std::min(static_cast<int>(_network.getWidth()) - 1, static_cast<int>(bestMatchingNeuronPos.x) + xRadius) };

            for (int x{ xMin }; x <= xMax; ++x) // row major
            {
                const MatrixPosition neuronPos{ static_cast<Coordinate>(x), static_cast<Coordinate>(y) };
                const int dx{ x - static_cast<int>(bestMatchingNeuronPos.x) };
                const std::size_t squaredGridDist{ squaredDy + static_cast<std::size_t>(dx * dx) };

                const FloatType influence{ _influenceLUT[static_cast<std::size_t>(squaredGridDist)] };

                Vector& neuron{ _network.getNeuron(neuronPos) };
                for (std::size_t i{}; i < Vector::getSize(); ++i)
                    neuron[i] += _learningRate * influence * (input[i] - neuron[i]);
            }
        }
    }
} // namespace lms::som