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

#include <optional>
#include <random>

#include "core/Random.hpp"

#include "Matrix.hpp"
#include "Vector.hpp"

namespace lms::som
{
    template<std::size_t DimensionCount, typename FloatType = float>
    class Network
    {
    public:
        using Vector = som::Vector<DimensionCount, FloatType>;

        Network() = default;

        // Init a network with default values
        Network(Coordinate width, Coordinate height);

        // Resize with default values (all values are set)
        void resize(Coordinate width, Coordinate height);

        template<typename RandomEngine>
        void randomize(RandomEngine& randomEngine, FloatType min, FloatType max);

        Coordinate getWidth() const { return _neurons.getWidth(); }
        Coordinate getHeight() const { return _neurons.getHeight(); }

        // Set weight for each dimension (default is 1 for each weight)
        void setWeights(const std::optional<Vector>& weights) { _weights = weights; }
        const Vector* getWeights() const { return _weights.has_value() ? &(_weights.value()) : nullptr; }

        // use this to manually construct a network without training
        void setNeuron(const MatrixPosition& position, const Vector& neuron);
        const Vector& getNeuron(const MatrixPosition& position) const;
        Vector& getNeuron(const MatrixPosition& position);

        MatrixPosition getBestMatchingNeuron(const Vector& input) const;

    private:
        Matrix<Vector> _neurons;
        std::optional<Vector> _weights;
    };

    template<std::size_t DimensionCount, typename FloatType>
    Network<DimensionCount, FloatType>::Network(Coordinate width, Coordinate height)
    {
        resize(width, height);
    }

    template<std::size_t DimensionCount, typename FloatType>
    void Network<DimensionCount, FloatType>::resize(Coordinate width, Coordinate height)
    {
        _neurons.resize(width, height);
    }

    template<std::size_t DimensionCount, typename FloatType>
    template<typename RandomEngine>
    void Network<DimensionCount, FloatType>::randomize(RandomEngine& randomEngine, FloatType min, FloatType max)
    {
        std::uniform_real_distribution<FloatType> distrib{ min, max };

        for (Coordinate y{}; y < _neurons.getHeight(); ++y)
        {
            for (Coordinate x{}; x < _neurons.getWidth(); ++x) // row major
                core::random::fillContainer(randomEngine, _neurons.get({ x, y }), min, max);
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
    typename Network<DimensionCount, FloatType>::Vector& Network<DimensionCount, FloatType>::getNeuron(const MatrixPosition& position)
    {
        return _neurons.get(position);
    }

    template<std::size_t DimensionCount, typename FloatType>
    MatrixPosition Network<DimensionCount, FloatType>::getBestMatchingNeuron(const Vector& input) const
    {
        if (_weights)
            return _neurons.getPositionMinDistance(SquaredEuclideanDistanceWithWeights{ input, *_weights });

        return _neurons.getPositionMinDistance(SquaredEuclideanDistance{ input });
    }
} // namespace lms::som
