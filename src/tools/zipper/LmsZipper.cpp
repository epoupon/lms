/*
 * Copyright (C) 2020 Emeric Poupon
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

#include <unistd.h>
#include <fstream>
#include <iostream>

#include "utils/Service.hpp"
#include "utils/StreamLogger.hpp"
#include "utils/Zipper.hpp"

int main(int argc, char* argv[])
{
	// log to stdout
	Service<Logger> logger {std::make_unique<StreamLogger>(std::cout)};

	if (argc < 2)
	{
		std::cerr << "Usage: <archive> <file> [...]" << std::endl;
		return EXIT_FAILURE;
	}

	std::filesystem::path zipPath {argv[1]};

	std::map<std::string, std::filesystem::path> files;
	for (int i {2}; i < argc; ++i)
		files.emplace(argv[i], argv[i]);


	std::cout << "Compressing " << files.size() << " files..." << std::endl;

	using namespace Zip;

	std::ofstream ofs {zipPath.string().c_str(), std::ios_base::binary};
	if (!ofs)
	{
		std::cerr << "Cannot open file '" << zipPath.string() << "' for writing";
		return EXIT_FAILURE;
	}

	Zipper zipper {files};

	while (!zipper.isComplete())
	{
		std::array<std::byte, 1000000> buffer;

		std::cout << "Call" << std::endl;
		std::size_t nbWrittenBytes {zipper.writeSome(buffer.data(), buffer.size())};
		std::cout << "nbWrittenBytes = " << nbWrittenBytes << std::endl;

		ofs.write(reinterpret_cast<const char*>(buffer.data()), nbWrittenBytes);
	}

	return EXIT_SUCCESS;
}
