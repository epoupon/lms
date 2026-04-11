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

#include "Matrix.hpp"
#include "Vector.hpp"

namespace lms::som
{
    template<std::size_t DimensionCount, typename FloatType = float>
    class Network
    {
    public:
        using Vector = som::Vector<DimensionCount, FloatType>;

        // Init a network with default values, default values
        Network(Coordinate width, Coordinate height);

        // Init a network with random values
        template<typename RandomEngine>
        Network(Coordinate width, Coordinate height, RandomEngine& randomEngine, FloatType min, FloatType max);

        Coordinate getWidth() const { return _neurons.getWidth(); }
        Coordinate getHeight() const { return _neurons.getHeight(); }

        // Set weight for each dimension (default is 1 for each weight)
        void setWeights(const Vector& weights) { _weights = weights; }
        const Vector& getWeights() const { return _weights; }

        // use this to manually construct a network without training
        void setNeuron(const MatrixPosition& position, const Vector& neuron);
        const Vector& getNeuron(const MatrixPosition& position) const;

        MatrixPosition getBestMatchingNeuron(const Vector& input) const;

        // Training interface
        void beginTraining(std::size_t epochCount);
        void beginNextEpoch();
        void train(const Vector& input);

    private:
        Matrix<Vector> _neurons;
        FloatType _initialRadius{};
        FloatType _initialLearningRate{};
        Vector _weights;

        // training data
        std::size_t _epochCount{};
        std::size_t _epoch{};
        FloatType _learningRate{};
        FloatType _sigma{};
        std::vector<FloatType> _influenceLUT;
    };
} // namespace lms::som

#include "private/NetworkImpl.hpp"
