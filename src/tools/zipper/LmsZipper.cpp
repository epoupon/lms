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

#include <array>
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
	{
		std::filesystem::path path {argv[i]};
		files.emplace(path.relative_path(), path);
	}


	std::cout << "Compressing " << files.size() << " files..." << std::endl;

	using namespace Zip;

	std::ofstream ofs {zipPath.string().c_str(), std::ios_base::binary};
	if (!ofs)
	{
		std::cerr << "Cannot open file '" << zipPath.string() << "' for writing";
		return EXIT_FAILURE;
	}

	try
	{
		Zipper zipper {files};

		Zip::SizeType nbTotalWrittenBytes {};
		while (!zipper.isComplete())
		{
			//std::array<std::byte, Zipper::minOutputBufferSize> buffer;
			std::array<std::byte, 65536> buffer;

			const Zip::SizeType nbWrittenBytes {zipper.writeSome(buffer.data(), buffer.size())};
			ofs.write(reinterpret_cast<const char*>(buffer.data()), nbWrittenBytes);
			nbTotalWrittenBytes += nbWrittenBytes;
		}

		if (nbTotalWrittenBytes != zipper.getTotalZipFile())
			std::cerr << "ERROR: actual size mismatch!" << std::endl;

		std::cout << "Total zip size = " << zipper.getTotalZipFile() << std::endl;
	}
	catch (const ZipperException& e)
	{
		std::cerr << "Caught Zipper exception: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
