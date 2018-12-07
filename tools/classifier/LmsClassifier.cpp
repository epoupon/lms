#include <stdlib.h>

#include <stdexcept>
#include <iostream>
#include <string>

#include "classifier/SOM.hpp"
#include "classifier/DataNormalizer.hpp"

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		std::cerr << "Usage: <file>" << std::endl;
		return EXIT_FAILURE;
	}

	auto iterationCount = std::stoul(argv[1]);

	using namespace SOM;

	SOM::Network network(5, 5, 2);

	network.dump(std::cout);

	std::vector< std::vector<double> > inputValues =
	{
		{ 160, 1 },
		{ 80, -1 },
		{ 80, -0.75 },
		{ 240, 0.5 },
		{ 120, -0.5 },
		{ 140, -0.5 },
	};

	std::cout << "Before normalization:" << std::endl;
	for (const auto& inputValue : inputValues)
	{
		std::cout << inputValue << std::endl;
	}
	std::cout << std::endl;

	std::cout << "After normalization:" << std::endl;

	SOM::DataNormalizer normalizer(2);

	normalizer.computeNormalizationFactors(inputValues);

	for (auto& inputValue : inputValues)
		normalizer.normalizeData(inputValue);

	for (const auto& inputValue : inputValues)
	{
		std::cout << inputValue << std::endl;
	}

	std::cout << std::endl;

	for (const auto& inputValue : inputValues)
	{
		auto res = network.classify(inputValue);
		std::cout << "Found at " << res.x << ", " << res.y << std::endl;
	}

	std::cout << "Training network for " << iterationCount << " iterations" << std::endl;

	network.train(inputValues, iterationCount);

	network.dump(std::cout);
	std::cout << "OK" << std::endl;

	for (const auto& inputValue : inputValues)
	{
		auto res = network.classify(inputValue);
		std::cout << "Found at " << res.x << ", " << res.y << std::endl;
	}

	return EXIT_SUCCESS;
}

