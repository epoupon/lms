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

#include <filesystem>
#include <map>

#include <Wt/WDateTime.h>

#include "Exception.hpp"
#include "utils/Crc32Calculator.hpp"

namespace Zip
{
	using SizeType = std::uint64_t;

	class ZipperException : public LmsException
	{
		using LmsException::LmsException;
	};

	// Very simple on-the-fly zip creator, "store" method only
	class Zipper
	{
		public:

			Zipper(const std::map<std::string, std::filesystem::path>& files, const Wt::WDateTime& lastModifiedTime = {});

			static constexpr SizeType minOutputBufferSize {64};
			SizeType writeSome(std::byte* buffer, SizeType bufferSize);
			bool isComplete() const;

			SizeType getTotalZipFile() const { return _totalZipSize; }

		private:
			void setComplete();

			SizeType writeLocalFileHeader(std::byte* buffer, SizeType bufferSize);
			SizeType writeLocalFileHeaderFileName(std::byte* buffer, SizeType bufferSize);
			SizeType writeLocalFileHeaderExtraFields(std::byte* buffer, SizeType bufferSize);
			SizeType writeFileData(std::byte* buffer, SizeType bufferSize);
			SizeType writeDataDescriptor(std::byte* buffer, SizeType bufferSize);
			SizeType writeCentralDirectoryHeader(std::byte* buffer, SizeType bufferSize);
			SizeType writeCentralDirectoryHeaderFileName(std::byte* buffer, SizeType bufferSize);
			SizeType writeCentralDirectoryHeaderExtraFields(std::byte* buffer, SizeType bufferSize);
			SizeType writeZip64EndOfCentralDirectoryRecord(std::byte* buffer, SizeType bufferSize);
			SizeType writeZip64EndOfCentralDirectoryLocator(std::byte* buffer, SizeType bufferSize);
			SizeType writeEndOfCentralDirectoryRecord(std::byte* buffer, SizeType bufferSize);

			struct FileContext
			{
				std::filesystem::path filePath;
				SizeType fileSize;
				Wt::WDateTime lastModifiedTime;
				Utils::Crc32Calculator fileCrc32;
				SizeType localFileHeaderOffset {};
			};

			using FileContainer = std::map<std::string, FileContext>;
			FileContainer _files;

			enum class WriteState
			{
				LocalFileHeader,
				LocalFileHeaderFileName,
				LocalFileHeaderExtraFields,
				FileData,
				DataDescriptor,
				CentralDirectoryHeader,
				CentralDirectoryHeaderFileName,
				CentralDirectoryHeaderExtraFields,
				Zip64EndOfCentralDirectoryRecord,
				Zip64EndOfCentralDirectoryLocator,
				EndOfCentralDirectoryRecord,
				Complete,
			};

			SizeType _totalZipSize {};
			WriteState _writeState {WriteState::LocalFileHeader};
			FileContainer::iterator _currentFile;
			SizeType _currentOffset {};
			SizeType _currentZipOffset {};
			SizeType _centralDirectoryOffset {};
			SizeType _centralDirectorySize {};
			SizeType _zip64EndOfCentralDirectoryRecordOffset {};
	};

} // namespace Zip

