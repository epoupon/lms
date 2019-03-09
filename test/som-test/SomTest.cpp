#include <cassert>
#include <assert.h>
#include <iostream>

#include "DataNormalizer.hpp"
#include "Network.hpp"

#define CHECK(pred)	assert((pred))

using namespace SOM;

int main(int argc, char* argv[])
{
	static const InputVector::value_type EPSILON = 0.01;
	{
		Matrix<int> testMatrix {2, 2, 123};
		assert((testMatrix[{0,0}] == 123));
		assert((testMatrix[{0,1}] == 123));
		assert((testMatrix[{1,0}] == 123));
		assert((testMatrix[{1,1}] == 123));
	}

	{
		InputVector test1 {2};
		test1[0] = 0;
		test1[1] = 1;

		InputVector test2 {2};
		test2[0] = 1;
		test2[1] = 0;

		InputVector test3 {test1};
		test3 += test2;
		assert(std::abs(test3[0] - 1) < EPSILON);
		assert(std::abs(test3[1] - 1) < EPSILON);
	}

	{
		Network network {2, 2, 1};

		const InputVector weights {1, 1};
		std::vector<InputVector> trainData {
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

		std::cout << "MEAN dist = " << network.computeRefVectorsDistanceMean() << std::endl;
		std::cout << "MEDIAN dist = " << network.computeRefVectorsDistanceMedian() << std::endl;

		auto distFunc {network.getDistanceFunc()};

		assert((std::abs(distFunc({1, 0}, {1, 1}, weights) - 1) < EPSILON));
		assert((std::abs(distFunc({1, 0}, {1, 2}, weights) - 4) < EPSILON));
		assert((std::abs(distFunc({1, 0}, {1, 0.33}, weights) - distFunc({1, 0.66}, {1, 1.}, weights)) < EPSILON));

		{
			std::set<Position> positions;
			for (const InputVector& data : trainData)
				positions.insert(network.getClosestRefVectorPosition(data));
			assert(positions.size() == 4);
		}

		{
			Position pos {network.getClosestRefVectorPosition(InputVector{1, 0.66})};
			for (std::size_t i {}; i < 40; ++i)
			{
				InputVector input {1, 130 + static_cast<InputVector::value_type>(i) };
				normalizer.normalizeData(input);

				assert( network.getClosestRefVectorPosition(input) == pos);
			}
		}

		{
			Position pos {network.getClosestRefVectorPosition(InputVector{1, 1})};
			for (std::size_t i {}; i < 40; ++i)
			{
				InputVector input {1, 180 + static_cast<InputVector::value_type>(i) };
				normalizer.normalizeData(input);

				assert( network.getClosestRefVectorPosition(input) == pos);
			}
		}

	}

	return 0;
}
