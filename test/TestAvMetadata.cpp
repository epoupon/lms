#include <stdlib.h>

#include <stdexcept>
#include <iostream>

#include "av/AvInfo.hpp"
#include "metadata/AvFormat.hpp"

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		std::cerr << "Usage: <file>" << std::endl;
		return EXIT_FAILURE;
	}

	try
	{
		Av::AvInit();

		MetaData::AvFormat parser;
		MetaData::Items items;

		if (!parser.parse(argv[1], items))
		{
			std::cout << "Parsing failed" << std::endl;
			return EXIT_FAILURE;
		}

		for (auto item : items)
		{
			switch (item.first)
			{
				case MetaData::Type::TrackNumber:
					std::cout << "Track: " << boost::any_cast<std::size_t>(item.second) << std::endl;
					break;

				case MetaData::Type::TotalTrack:
					std::cout << "TotalTrack: " << boost::any_cast<std::size_t>(item.second) << std::endl;
					break;

				case MetaData::Type::DiscNumber:
					std::cout << "Disc: " << boost::any_cast<std::size_t>(item.second) << std::endl;
					break;

				case MetaData::Type::TotalDisc:
					std::cout << "TotalDisc: " << boost::any_cast<std::size_t>(item.second) << std::endl;

				default:
					break;
			}
		}

		return EXIT_SUCCESS;
	}
	catch (std::exception& e)
	{
		std::cerr << "Caught exception: " << e.what();
		return EXIT_FAILURE;
	}

}

