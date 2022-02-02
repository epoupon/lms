/*
 * Copyright (C) 2019 Emeric Poupon
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

#include <unordered_set>
#include <gtest/gtest.h>
#include "som/DataNormalizer.hpp"
#include "som/Network.hpp"

using namespace SOM;

static constexpr InputVector::value_type EPSILON = 0.01;

TEST(som, Matrix)
{
	{
		Matrix<int> testMatrix {2, 2, 123};
		{
			const Position pos {0, 0};
			EXPECT_EQ(testMatrix[pos],  123);
		}
		{
			const Position pos {0, 1};
			EXPECT_EQ(testMatrix[pos], 123);
		}
		{
			const Position pos {1, 0};
			EXPECT_EQ(testMatrix[pos], 123);
		}
		{
			const Position pos {1, 1};
			EXPECT_EQ(testMatrix[pos], 123);
		}
	}
}

TEST(som, InputVector)
{
	{
		InputVector test1 {2};
		test1[0] = 0;
		test1[1] = 1;

		InputVector test2 {2};
		test2[0] = 1;
		test2[1] = 0;

		InputVector test3 {test1};
		test3 += test2;
		EXPECT_LT(std::abs(test3[0] - 1), EPSILON);
		EXPECT_LT(std::abs(test3[1] - 1), EPSILON);
	}
}

TEST(som, Network)
{
	Network network {2, 2, 1};

	const InputVector weights {1, 1};
	std::vector<InputVector> trainData
	{
		{ 1, 50 },
		{ 1, 100 },
		{ 1, 150 },
		{ 1, 200 },
	};

	DataNormalizer normalizer {1};
	normalizer.computeNormalizationFactors(trainData);
	for (auto& data: trainData)
		normalizer.normalizeData(data);

	network.dump(std::cout);
	network.train(trainData, 20);
	network.dump(std::cout);

	auto distFunc {network.getDistanceFunc()};

	EXPECT_LT((std::abs(distFunc({1, 0}, {1, 1}, weights) - 1)), EPSILON);
	EXPECT_LT((std::abs(distFunc({1, 0}, {1, 2}, weights) - 4)), EPSILON);
	EXPECT_LT(std::abs(distFunc({1, 0}, {1, 0.33}, weights) - distFunc({1, 0.66}, {1, 1.}, weights)), EPSILON);

	{
		std::unordered_set<Position> positions;
		for (const InputVector& data : trainData)
			positions.insert(network.getClosestRefVectorPosition(data));

		EXPECT_EQ(positions.size(), 4);
	}

	{
		Position pos {network.getClosestRefVectorPosition(InputVector{1, 0.66})};
		for (std::size_t i {}; i < 40; ++i)
		{
			InputVector input {1, 130 + static_cast<InputVector::value_type>(i) };
			normalizer.normalizeData(input);

			EXPECT_EQ(network.getClosestRefVectorPosition(input), pos);
		}
	}

	{
		Position pos {network.getClosestRefVectorPosition(InputVector{1, 1})};
		for (std::size_t i {}; i < 40; ++i)
		{
			InputVector input {1, 180 + static_cast<InputVector::value_type>(i) };
			normalizer.normalizeData(input);

			EXPECT_EQ(network.getClosestRefVectorPosition(input), pos);
		}
	}
}

int main(int argc, char **argv)
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
