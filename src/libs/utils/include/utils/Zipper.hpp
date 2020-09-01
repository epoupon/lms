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

#pragma once

#include <map>
#include <set>
#include <filesystem>

#include <Wt/WDateTime.h>

#include "Exception.hpp"
#include "utils/Crc32Calculator.hpp"

namespace Zip
{

	class ZipperException : public LmsException
	{
		using LmsException::LmsException;
	};

	// Very simple on-the-fly zip creator, "store" method only
	class Zipper
	{
		public:

			using SizeZype = std::uint64_t;

			Zipper(const std::map<std::string, std::filesystem::path>& files, const Wt::WDateTime& lastModifiedTime = {});

			static constexpr std::size_t minOutputBufferSize = 64;
			std::size_t writeSome(std::byte* buffer, std::size_t bufferSize);
			bool isComplete() const;

			SizeZype getTotalZipFile() const { return _totalZipSize; }

		private:
			void setComplete();

			std::size_t writeLocalFileHeader(std::byte* buffer, std::size_t bufferSize);
			std::size_t writeLocalFileHeaderFileName(std::byte* buffer, std::size_t bufferSize);
			std::size_t writeFileData(std::byte* buffer, std::size_t bufferSize);
			std::size_t writeDataDescriptor(std::byte* buffer, std::size_t bufferSize);
			std::size_t writeCentralDirectoryHeader(std::byte* buffer, std::size_t bufferSize);
			std::size_t writeCentralDirectoryHeaderFileName(std::byte* buffer, std::size_t bufferSize);
			std::size_t writeEndOfCentralDirectoryRecord(std::byte* buffer, std::size_t bufferSize);

			struct FileContext
			{
				std::filesystem::path filePath;
				std::size_t fileSize;
				Wt::WDateTime lastModifiedTime;
				Utils::Crc32Calculator fileCrc32;
				std::size_t localFileHeaderOffset {};
			};

			using FileContainer = std::map<std::string, FileContext>;
			FileContainer _files;

			enum class WriteState
			{
				LocalFileHeader,
				LocalFileHeaderFileName,
				FileData,
				DataDescriptor,
				CentralDirectoryHeader,
				CentralDirectoryHeaderFileName,
				EndOfCentralDirectoryRecord,
				Complete,
			};

			SizeZype _totalZipSize {};
			WriteState _writeState {WriteState::LocalFileHeader};
			FileContainer::iterator _currentFile;
			std::size_t _currentOffset {};
			std::size_t _currentZipOffset {};
			std::size_t _centralDirectoryOffset {};
			std::size_t _centralDirectorySize {};
	};

} // namespace Zip

