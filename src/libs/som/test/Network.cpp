/*
 * Copyright (C) 2026 Emeric Poupon
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

#include <random>

#include <gtest/gtest.h>

#include "som/Network.hpp"
#include "som/Trainer.hpp"

namespace lms::som
{
    TEST(Network, DefaultConstruction)
    {
        constexpr std::size_t dimensionCount{ 10 };
        Network<dimensionCount> network;

        EXPECT_EQ(network.getWidth(), 0);
        EXPECT_EQ(network.getHeight(), 0);

        for (const auto weight : network.getWeights())
            EXPECT_EQ(weight, 1.F);
    }

    TEST(Network, Randomize)
    {
        constexpr std::size_t dimensionCount{ 10 };

        Network<dimensionCount> network{ 5, 10 };
        EXPECT_EQ(network.getWidth(), 5);
        EXPECT_EQ(network.getHeight(), 10);

        constexpr float min{ 1.F };
        constexpr float max{ 2.F };
        std::minstd_rand randomEngine{ 42 };
        network.randomize(randomEngine, min, max);

        for (Coordinate x{}; x < network.getWidth(); ++x)
        {
            for (Coordinate y{}; y < network.getHeight(); ++y)
            {
                for (const auto val : network.getNeuron({ x, y }))
                {
                    EXPECT_GE(val, min);
                    EXPECT_LE(val, max);
                }
            }
        }
    }

    TEST(Network, GetBestMatchingNeuron)
    {
        constexpr std::size_t dimensionCount{ 2 };

        using Network = Network<dimensionCount>;
        using Vector = Vector<dimensionCount>;
        Network network{ 2, 2 };

        network.setNeuron({ 0, 0 }, Vector{ 0.F, 0.F });
        network.setNeuron({ 1, 0 }, Vector{ 10.F, 10.F });
        network.setNeuron({ 0, 1 }, Vector{ 1.F, 1.F });
        network.setNeuron({ 1, 1 }, Vector{ 5.F, 5.F });

        EXPECT_EQ(network.getBestMatchingNeuron({ 0.25F, 0.25F }), (MatrixPosition{ 0, 0 }));
        EXPECT_EQ(network.getBestMatchingNeuron({ 9.F, 9.F }), (MatrixPosition{ 1, 0 }));
    }

    TEST(Network, BestMatchingNeuronRespectsWeights)
    {
        constexpr std::size_t dimensionCount{ 2 };

        using Network = Network<dimensionCount>;
        using Vector = Vector<dimensionCount>;
        Network network{ 2, 1 };

        network.setNeuron({ 0, 0 }, Vector{ 0.F, 5.F });
        network.setNeuron({ 1, 0 }, Vector{ 2.F, 0.F });

        const Vector input{ 1.5F, 4.F };

        network.setWeights(Vector{ 1.F, 1.F });
        EXPECT_EQ(network.getBestMatchingNeuron(input), (MatrixPosition{ 0, 0 }));

        network.setWeights(Vector{ 1.F, 0.F });
        EXPECT_EQ(network.getBestMatchingNeuron(input), (MatrixPosition{ 1, 0 }));
    }

    TEST(Network, TrainUpdatesBestMatchingNeuron)
    {
        constexpr std::size_t dimensionCount{ 2 };
        constexpr std::size_t epochCount{ 10 };

        using Network = Network<dimensionCount>;
        using Vector = Vector<dimensionCount>;
        Network network{ 1, 1 };

        const Vector initialNeuron{ 0.F, 0.F };
        const Vector input{ 5.F, 5.F };

        network.setNeuron({ 0, 0 }, initialNeuron);

        {
            Trainer trainer{ network, TrainerParams{ .epochCount = epochCount } };

            for (std::size_t epoch = 0; epoch < epochCount; ++epoch)
            {
                trainer.beginEpoch();
                trainer.train(input);
            }
        }

        const Vector trainedNeuron{ network.getNeuron({ 0, 0 }) };

        // The neuron should have moved towards the input
        EXPECT_GT(trainedNeuron[0], initialNeuron[0]);
        EXPECT_GT(trainedNeuron[1], initialNeuron[1]);

        // It shouldn't completely match the input (learning is gradual)
        EXPECT_LT(trainedNeuron[0], input[0]);
        EXPECT_LT(trainedNeuron[1], input[1]);
    }

    TEST(Network, Train)
    {
        constexpr std::size_t dimensionCount{ 2 };
        using Network = Network<dimensionCount>;
        using Vector = Vector<dimensionCount>;

        constexpr std::size_t width{ 10 };
        constexpr std::size_t height{ 10 };
        constexpr std::size_t epochCount{ 40 };

        std::minstd_rand randomEngine{ 0 };
        Network network{ width, height };
        network.randomize(randomEngine, 0.F, 1.F);

        const Vector inputA{ 0.1f, 0.1f };
        const Vector inputB{ 0.9f, 0.9f };

        EXPECT_GT(inputA.computeEuclideanSquaredDistance(inputB), 1.0F);

        {
            Trainer trainer{ network, TrainerParams{ .epochCount = epochCount } };

            for (std::size_t epoch = 0; epoch < epochCount; ++epoch)
            {
                trainer.beginEpoch();
                trainer.train(inputA);
                trainer.train(inputB);
            }
        }

        const MatrixPosition bestMatchingNeuronAPos{ network.getBestMatchingNeuron(inputA) };
        const MatrixPosition bestMatchingNeuronBPos{ network.getBestMatchingNeuron(inputB) };

        EXPECT_NE(bestMatchingNeuronAPos, bestMatchingNeuronBPos);

        const Vector& bestMatchingNeuronA{ network.getNeuron(bestMatchingNeuronAPos) };
        const Vector& bestMatchingNeuronB{ network.getNeuron(bestMatchingNeuronBPos) };

        EXPECT_LT(bestMatchingNeuronA.computeEuclideanSquaredDistance(inputA), 0.1F);
        EXPECT_LT(bestMatchingNeuronB.computeEuclideanSquaredDistance(inputB), 0.1F);
    }
} // namespace lms::som
