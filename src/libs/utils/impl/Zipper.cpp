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

#include "utils/Zipper.hpp"

#include <cstddef>
#include <fstream>

#include "utils/Path.hpp"

// Done using specs from https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT

namespace Zip
{

	class ZipHeader
	{
		public:
			ZipHeader(std::byte* buffer, std::size_t bufferSize)
				: _buffer {buffer}
				, _bufferSize {bufferSize}
				{}

			enum GeneralPurposeFlag : std::uint16_t
			{
				UseDataDescriptor	= 1 << 3,
				LanguageEncoding	= 1 << 11,
			};

			enum CompressionMethod : std::uint16_t
			{
				NoCompression = 0,
			};

			static constexpr std::size_t UnknownCrc32 {0};
			static constexpr std::size_t UnknownFileSize {0};


		protected:
			void write8(std::size_t offset, std::uint8_t value);
			void write16(std::size_t offset, std::uint16_t value);
			void write32(std::size_t offset, std::uint32_t value);

		private:
			std::byte*	_buffer {};
			std::size_t _bufferSize {};
	};

	void
	ZipHeader::write8(std::size_t offset, std::uint8_t value)
	{
		_buffer[offset] = static_cast<std::byte>(value);
	}

	void
	ZipHeader::write16(std::size_t offset, std::uint16_t value)
	{
		_buffer[offset] = static_cast<std::byte>(value & 0xff);
		_buffer[offset + 1] = static_cast<std::byte>(value >> 8);
	}

	void
	ZipHeader::write32(std::size_t offset, std::uint32_t value)
	{
		_buffer[offset] = static_cast<std::byte>(value & 0xff);
		_buffer[offset + 1] = static_cast<std::byte>((value >> 8) & 0xff);
		_buffer[offset + 2] = static_cast<std::byte>((value >> 16) & 0xff);
		_buffer[offset + 3] = static_cast<std::byte>(value >> 24);
	}

	class LocalFileHeader : public ZipHeader
	{
		public:
			using ZipHeader::ZipHeader;

			// Setters
			void setSignature() { write32(0, 0x04034b50); }
			void setVersionNeededToExtract(unsigned major, unsigned minor) { assert(minor < 10); write16(4, major*10 + minor); }
			void setGeneralPurposeFlags(std::uint16_t flags) { write16(6, flags); }
			void setCompressionMethod(CompressionMethod compressionMethod) { write16(8, compressionMethod); }
			void setLastModifiedDateTime()
			{ // TODO
				write16(10, 0); // time
				write16(12, 0); // date
			}
			void setCrc32UncompressedData(std::uint32_t crc) { write32(14, crc); }
			void setCompressedSize(std::size_t size) { write32(18, size); }
			void setUncompressedSize(std::size_t size) { write32(22, size); }
			void setFileNameLength(std::size_t size) { write16(26, size); }
			void setExtraFieldLength(std::size_t size) { write16(28, size); }
			static constexpr std::size_t getHeaderSize() { return 30; }
	};

	class DataDescriptor : public ZipHeader
	{
		public:
			using ZipHeader::ZipHeader;

			void setSignature() { write32(0, 0x08074b50 ); }
			void setCrc32UncompressedData(std::uint32_t crc32) { write32(4, crc32); }
			void setCompressedSize(std::size_t size) { write32(8, size); }
			void setUncompressedSize(std::size_t size) { write32(12, size); }
			static constexpr std::size_t getHeaderSize() { return 16; }
	};

	class CentralDirectoryHeader : public ZipHeader
	{
		public:
			using ZipHeader::ZipHeader;

			void setSignature()	{ write32(0, 0x02014b50); }
			void setVersionMadeBy(unsigned major, unsigned minor) { assert(minor < 10); write16(4, major * 10 + minor); }
			void setVersionNeededToExtract(unsigned major, unsigned minor) { assert(minor < 10); write16(6, major*10 + minor); }
			void setGeneralPurposeFlags(std::uint16_t flags) { write16(8, flags); }
			void setCompressionMethod(CompressionMethod method) { write16(10, method); }
			void setLastModifiedDateTime()
			{
				write16(12, 0); // time
				write16(14, 0); // date
			}
			void setCrc32UncompressedData(std::uint32_t crc32) { write32(16, crc32); }
			void setCompressedSize(std::size_t size) { write32(20, size); }
			void setUncompressedSize(std::size_t size) { write32(24, size); }
			void setFileNameLength(std::size_t size) { write16(28, size); }
			void setExtraFieldLength(std::size_t size) { write16(30, size); }
			void setFileCommentLength(std::size_t size) { write16(32, size); }
			void setDiskNumber(std::size_t number) { write16(34, number); }
			void setInternalFileAttributes(std::uint16_t attributes) { write16(36, attributes); }
			void setExternalFileAttributes(std::uint16_t attributes) { write32(38, attributes); }
			void setRelativeFileHeaderOffset(std::size_t offset) { write32(42, offset); }
			static constexpr std::size_t getHeaderSize() { return 46; }
	};

	class EndOfCentralDirectoryRecord : public ZipHeader
	{
		public:
			using ZipHeader::ZipHeader;

			void setSignature() { write32(0, 0x06054b50); }
			void setDiskNumber(unsigned number) { write16(4, number); }
			void setCentralDirectoryDiskNumber(unsigned number) { write16(6, number); }
			void setNbDiskCentralDirectoryRecords(unsigned number) { write16(8, number); }
			void setNbCentralDirectoryRecords(unsigned number) { write16(10, number); }
			void setCentralDirectorySize(std::size_t size) { write32(12, size); }
			void setCentralDirectoryOffset(std::size_t offset) { write32(16, offset); }
			void setCommentLength(std::size_t length) { write16(20, length); }
			static constexpr std::size_t getHeaderSize() { return 22; }
	};

	Zipper::Zipper(const std::map<std::string, std::filesystem::path>& files)
	{
		for (const auto& [filename, filePath] : files)
		{
			FileContext fileContext;
			fileContext.filePath = filePath;

			std::error_code ec;
			fileContext.fileSize = std::filesystem::file_size(filePath, ec);
			if (ec)
				throw ZipperException {"Cannot get file size for '" + filePath.string() + "': " + ec.message()};

			_files[filename] = std::move(fileContext);
		}

		_currentFile = std::begin(_files);
	}

	std::size_t
	Zipper::writeSome(std::byte* buffer, std::size_t bufferSize)
	{
		// make sure we have some room for the headers
		assert(bufferSize >= minOutputBufferSize);

		std::size_t nbTotalWrittenBytes {};

		while (!isComplete() && (bufferSize >= minOutputBufferSize))
		{
			std::size_t nbWrittenBytes {};

			switch (_writeState)
			{
				case WriteState::LocalFileHeader:
					nbWrittenBytes = writeLocalFileHeader(buffer, bufferSize);
					break;

				case WriteState::LocalFileHeaderFileName:
					nbWrittenBytes = writeLocalFileHeaderFileName(buffer, bufferSize);
					break;

				case WriteState::FileData:
					nbWrittenBytes = writeFileData(buffer, bufferSize);
					break;

				case WriteState::DataDescriptor:
					nbWrittenBytes = writeDataDescriptor(buffer, bufferSize);
					break;

				case WriteState::CentralDirectoryHeader:
					nbWrittenBytes = writeCentralDirectoryHeader(buffer, bufferSize);
					break;

				case WriteState::CentralDirectoryHeaderFileName:
					nbWrittenBytes = writeCentralDirectoryHeaderFileName(buffer, bufferSize);
					break;

				case WriteState::EndOfCentralDirectoryRecord:
					nbWrittenBytes = writeEndOfCentralDirectoryRecord(buffer, bufferSize);
					break;

				case WriteState::Complete:
					break;
			}

			buffer += nbWrittenBytes;
			bufferSize -= nbWrittenBytes;
			_currentZipOffset += nbWrittenBytes;
			nbTotalWrittenBytes += nbWrittenBytes ;
		}

		return nbTotalWrittenBytes;
	}

	bool
	Zipper::isComplete() const
	{
		return _writeState == WriteState::Complete;
	}

	std::size_t
	Zipper::writeLocalFileHeader(std::byte* buffer, std::size_t bufferSize)
	{
		static_assert(LocalFileHeader::getHeaderSize() <= minOutputBufferSize);

		assert(bufferSize >= minOutputBufferSize);

		if (_currentFile == std::cend(_files))
		{
			_currentFile = std::begin(_files);
			_writeState = WriteState::CentralDirectoryHeader;
			return 0;
		}

		LocalFileHeader header {buffer, bufferSize};

		header.setSignature();
		header.setVersionNeededToExtract(1, 0);
		header.setGeneralPurposeFlags(ZipHeader::GeneralPurposeFlag::LanguageEncoding | ZipHeader::GeneralPurposeFlag::UseDataDescriptor);
		header.setCompressionMethod(ZipHeader::CompressionMethod::NoCompression);
		header.setCrc32UncompressedData(ZipHeader::UnknownCrc32);
		header.setCompressedSize(ZipHeader::UnknownFileSize);
		header.setUncompressedSize(ZipHeader::UnknownFileSize);
		header.setLastModifiedDateTime(); // getLastWriteTime(*_currentFile));
		header.setFileNameLength(_currentFile->first.size());
		header.setExtraFieldLength(0);

		_writeState = WriteState::LocalFileHeaderFileName;
		_currentFile->second.localFileHeaderOffset = _currentZipOffset;

		return header.getHeaderSize();
	}

	std::size_t
	Zipper::writeLocalFileHeaderFileName(std::byte* buffer, std::size_t bufferSize)
	{
		assert(_currentFile != std::end(_files));

		const std::string& fileName {_currentFile->first};

		assert(_currentOffset <= fileName.size());
		if (_currentOffset == fileName.size())
		{
			_writeState = WriteState::FileData;
			_currentOffset = 0;
			return 0;
		}

		const std::size_t nbBytesToCopy {std::min<std::size_t>(fileName.size() - _currentOffset, bufferSize)};

		std::copy(std::next(std::begin(fileName), _currentOffset), std::next(std::begin(fileName), nbBytesToCopy), reinterpret_cast<unsigned char*>(buffer));

		_currentOffset += nbBytesToCopy;
		return nbBytesToCopy;
	}

	std::size_t
	Zipper::writeFileData(std::byte* buffer, std::size_t bufferSize)
	{
		assert(_currentFile != std::end(_files));

		if (_currentOffset == _currentFile->second.fileSize)
		{
			_currentOffset = 0;
			_writeState = WriteState::DataDescriptor;
			return 0;
		}

		const std::string filePath {_currentFile->second.filePath.string()};

		std::ifstream ifs {filePath.c_str(), std::ios_base::binary};
		if (!ifs)
			throw ZipperException {"File '" + filePath + "' does no longer exist!"};

		ifs.seekg(0, std::ios::end);
		const ::uint64_t fileSize {static_cast<::uint64_t>(ifs.tellg())};
		ifs.seekg(0, std::ios::beg);

		if (fileSize != _currentFile->second.fileSize)
			throw ZipperException {"File '" + filePath + "': size mismatch!"};

		const std::size_t nbBytesToRead {std::min(fileSize - _currentOffset, bufferSize)};

		ifs.seekg(_currentOffset, std::ios::beg);
		ifs.read(reinterpret_cast<char*>(buffer), nbBytesToRead );
		const ::uint64_t actualReadSize {static_cast<::uint64_t>(ifs.gcount())};

		_currentFile->second.fileCrc32.processBytes(buffer, actualReadSize);
		_currentOffset += actualReadSize;

		return actualReadSize;
	}

	std::size_t
	Zipper::writeDataDescriptor(std::byte* buffer, std::size_t bufferSize)
	{
		assert(bufferSize >= minOutputBufferSize);
		static_assert(DataDescriptor::getHeaderSize() <= minOutputBufferSize);

		assert(_currentFile != std::end(_files));

		DataDescriptor desc {buffer, bufferSize};
		desc.setSignature();
		desc.setCrc32UncompressedData(_currentFile->second.fileCrc32.getResult());
		desc.setCompressedSize(_currentFile->second.fileSize);
		desc.setUncompressedSize(_currentFile->second.fileSize);

		++_currentFile;
		_writeState = WriteState::LocalFileHeader;

		return desc.getHeaderSize();
	}

	std::size_t
	Zipper::writeCentralDirectoryHeader(std::byte* buffer, std::size_t bufferSize)
	{
		assert(bufferSize >= minOutputBufferSize);
		static_assert(CentralDirectoryHeader::getHeaderSize() <= minOutputBufferSize);

		if (_currentFile == std::begin(_files))
			_centralDirectoryOffset = _currentZipOffset;

		if (_currentFile == std::end(_files))
		{
			_writeState = WriteState::EndOfCentralDirectoryRecord;
			_currentFile = std::begin(_files);
			return 0;
		}

		CentralDirectoryHeader header {buffer, bufferSize};
		header.setSignature();
		header.setVersionMadeBy(2, 0);
		header.setVersionNeededToExtract(1, 0);
		header.setGeneralPurposeFlags(ZipHeader::GeneralPurposeFlag::LanguageEncoding | ZipHeader::GeneralPurposeFlag::UseDataDescriptor);
		header.setCompressionMethod(ZipHeader::CompressionMethod::NoCompression);
		header.setCompressedSize(_currentFile->second.fileSize);
		header.setUncompressedSize(_currentFile->second.fileSize);
		header.setLastModifiedDateTime(); // getLastWriteTime(*_currentFile));
		header.setCrc32UncompressedData(_currentFile->second.fileCrc32.getResult());
		header.setFileNameLength(_currentFile->first.size());
		header.setExtraFieldLength(0);
		header.setFileCommentLength(0);
		header.setDiskNumber(0);
		header.setInternalFileAttributes(0);
		header.setExternalFileAttributes(0);
		header.setRelativeFileHeaderOffset(_currentFile->second.localFileHeaderOffset);

		_writeState = WriteState::CentralDirectoryHeaderFileName;
		_centralDirectorySize += header.getHeaderSize();

		return header.getHeaderSize();
	}

	std::size_t
	Zipper::writeCentralDirectoryHeaderFileName(std::byte* buffer, std::size_t bufferSize)
	{
		const std::string& fileName {_currentFile->first};

		assert(_currentOffset <= fileName.size());
		if (_currentOffset == fileName.size())
		{
			_currentOffset = 0;
			++_currentFile;
			_writeState  = WriteState::CentralDirectoryHeader;

			return 0;
		}

		const std::size_t nbBytesToCopy {std::min<std::size_t>(fileName.size() - _currentOffset, bufferSize)};

		std::copy(std::next(std::begin(fileName), _currentOffset), std::next(std::begin(fileName), nbBytesToCopy), reinterpret_cast<unsigned char*>(buffer));

		_currentOffset += nbBytesToCopy;
		_centralDirectorySize += nbBytesToCopy;
		return nbBytesToCopy;
	}


	std::size_t
	Zipper::writeEndOfCentralDirectoryRecord(std::byte* buffer, std::size_t bufferSize)
	{
		assert(bufferSize >= minOutputBufferSize);
		static_assert(EndOfCentralDirectoryRecord::getHeaderSize() <= minOutputBufferSize);

		EndOfCentralDirectoryRecord record {buffer, bufferSize};

		record.setSignature();
		record.setDiskNumber(0);
		record.setCentralDirectoryDiskNumber(0);
		record.setNbDiskCentralDirectoryRecords(_files.size());
		record.setNbCentralDirectoryRecords(_files.size());
		record.setCentralDirectorySize(_centralDirectorySize);
		record.setCentralDirectoryOffset(_centralDirectoryOffset);
		record.setCommentLength(0);

		_writeState = WriteState::Complete;
		return record.getHeaderSize();
	}

} // namespace Zip
