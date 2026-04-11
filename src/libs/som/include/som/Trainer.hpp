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

#include <vector>

#include "Matrix.hpp"
#include "Vector.hpp"

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
        std::vector<FloatType> _influenceLUT;
    };

    namespace detail
    {
        template<typename FloatType>
        FloatType computeSquaredDist(const MatrixPosition& a, const MatrixPosition& b)
        {
            const FloatType dx{ static_cast<FloatType>(b.x) - static_cast<FloatType>(a.x) };
            const FloatType dy{ static_cast<FloatType>(b.y) - static_cast<FloatType>(a.y) };
            return dx * dx + dy * dy;
        }
    } // namespace detail

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
            constexpr FloatType learninRateFinal{ 0.01 };
            _learningRate = _initialLearningRate * std::pow(learninRateFinal / _initialLearningRate, static_cast<FloatType>(_epoch) / static_cast<FloatType>(_epochCount));
        }
        {
            constexpr FloatType sigmaFinal{ 1 };
            _sigma = _initialRadius * std::pow(sigmaFinal / _initialRadius, static_cast<FloatType>(_epoch) / static_cast<FloatType>(_epochCount));
        }

        {
            const FloatType squaredSigma{ _sigma * _sigma };
            const std::size_t maxSquaredDist{ static_cast<std::size_t>(std::ceil(3 * squaredSigma)) };
            const FloatType invTwoSquaredSigma{ FloatType{ 1 } / (FloatType{ 2 } * squaredSigma) };

            _influenceLUT.resize(maxSquaredDist + 1);
            for (std::size_t squaredDist{}; squaredDist <= maxSquaredDist; ++squaredDist)
                _influenceLUT[squaredDist] = std::exp(-static_cast<FloatType>(squaredDist) * invTwoSquaredSigma);
        }

        _epoch += 1;
    }

    template<typename Network>
    void Trainer<Network>::train(const Vector& input)
    {
        const MatrixPosition bestMatchingNeuronPos{ _network.getBestMatchingNeuron(input) };
        for (Coordinate y{}; y < _network.getHeight(); ++y)
        {
            for (Coordinate x{}; x < _network.getWidth(); ++x) // row major
            {
                const MatrixPosition neuronPos{ x, y };
                const FloatType squaredGridDist{ detail::computeSquaredDist<FloatType>(neuronPos, bestMatchingNeuronPos) };

                if (squaredGridDist >= _influenceLUT.size())
                    continue;

                const FloatType influence{ _influenceLUT[static_cast<std::size_t>(squaredGridDist)] };

                Vector& neuron{ _network.getNeuron(neuronPos) };
                for (std::size_t i{}; i < Vector::getSize(); ++i)
                    neuron[i] += _learningRate * influence * (input[i] - neuron[i]);
            }
        }
    }
} // namespace lms::som