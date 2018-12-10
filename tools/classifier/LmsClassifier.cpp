#include <stdlib.h>

#include <stdexcept>
#include <iostream>
#include <string>

#include "classifier/SOM.hpp"
#include "classifier/DataNormalizer.hpp"
#include "classifier/Clusterer.hpp"

int main(int argc, char *argv[])
{

	if (argc != 2)
	{
		std::cerr << "Usage: <file>" << std::endl;
		return EXIT_FAILURE;
	}

	auto iterationCount = std::stoul(argv[1]);

	std::vector< std::pair<std::vector<SOM::InputVector::value_type>, std::string> > inputValues =
	{
		{{ 160, 1 }, { "banane" }},
		{{ 80, -1 }, { "poire" }},
		{{ 80, -0.75 }, {"pocolat"}},
		{{ 240, 0.5 }, {"abricot"}},
		{{ 240, -0.5 }, {"peche"}},
		{{ 120, -0.5 }, {"fraise"}},
		{{ 140, -0.5 }, {"myrtille"}},
	};

	Clusterer<std::string> classifier(inputValues, 2, iterationCount);

	std::cout << "Clusterer :" << std::endl;
	classifier.dump(std::cout);

	std::cout << "Classify 195, 0.35 = " << std::endl;
	for (const auto& val : classifier.getClusterValues({195, 0.35}))
		std::cout << val << " " << std::endl;

	return EXIT_SUCCESS;
}

