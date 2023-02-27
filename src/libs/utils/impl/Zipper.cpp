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

#include <Wt/WDate.h>
#include <Wt/WTime.h>

#include "utils/Path.hpp"

// Done using specs from https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT

namespace Zip
{

	class ZipHeader
	{
		public:
			constexpr ZipHeader(std::byte* buffer, SizeType)
				: _buffer {buffer}
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

			static constexpr std::uint32_t UnknownCrc32 {0};
			static constexpr SizeType UnknownFileSize {0};

			struct Version
			{
				unsigned major;
				unsigned minor;
			};
			static constexpr Version VersionMadeBy {4, 5};
			static constexpr Version VersionNeededToExtract {4, 5};

		protected:
			void write8(SizeType offset, std::uint8_t value);
			void write16(SizeType offset, std::uint16_t value);
			void write32(SizeType offset, std::uint32_t value);
			void write64(SizeType offset, std::uint64_t value);
			void writeDateTime(SizeType offset, const Wt::WDateTime& time);

		private:
			std::byte*	_buffer {};
	};

	void
	ZipHeader::write8(SizeType offset, std::uint8_t value)
	{
		_buffer[offset] = static_cast<std::byte>(value);
	}

	void
	ZipHeader::write16(SizeType offset, std::uint16_t value)
	{
		_buffer[offset] = static_cast<std::byte>(value & 0xff);
		_buffer[offset + 1] = static_cast<std::byte>(value >> 8);
	}

	void
	ZipHeader::write32(SizeType offset, std::uint32_t value)
	{
		_buffer[offset] = static_cast<std::byte>(value & 0xff);
		_buffer[offset + 1] = static_cast<std::byte>((value >> 8) & 0xff);
		_buffer[offset + 2] = static_cast<std::byte>((value >> 16) & 0xff);
		_buffer[offset + 3] = static_cast<std::byte>(value >> 24);
	}

	void
	ZipHeader::write64(SizeType offset, std::uint64_t value)
	{
		_buffer[offset] = static_cast<std::byte>(value & 0xff);
		_buffer[offset + 1] = static_cast<std::byte>((value >> 8) & 0xff);
		_buffer[offset + 2] = static_cast<std::byte>((value >> 16) & 0xff);
		_buffer[offset + 3] = static_cast<std::byte>((value >> 24) & 0xff);
		_buffer[offset + 4] = static_cast<std::byte>((value >> 32) & 0xff);
		_buffer[offset + 5] = static_cast<std::byte>((value >> 40) & 0xff);
		_buffer[offset + 6] = static_cast<std::byte>((value >> 48) & 0xff);
		_buffer[offset + 7] = static_cast<std::byte>(value >> 56);
	}

	void
	ZipHeader::writeDateTime(SizeType offset, const Wt::WDateTime& dateTime)
	{
		std::uint32_t encodedDateTime{};

		// Date
		encodedDateTime |= ((dateTime.date().year() - 1980) << 25);
		encodedDateTime |= (dateTime.date().month() << 21);
		encodedDateTime |= (dateTime.date().day() << 16);

		// Time
		encodedDateTime |= (dateTime.time().hour() << 11);
		encodedDateTime |= (dateTime.time().minute() << 5);
		encodedDateTime |= (dateTime.time().second() << 1);

		write32(offset, encodedDateTime);
	}

	class LocalFileHeader : public ZipHeader
	{
		public:
			using ZipHeader::ZipHeader;

			void setSignature() { write32(0, 0x04034b50); }
			void setVersionNeededToExtract(Version version) { assert(version.minor < 10); write16(4, version.major*10 + version.minor); }
			void setGeneralPurposeFlags(std::uint16_t flags) { write16(6, flags); }
			void setCompressionMethod(CompressionMethod compressionMethod) { write16(8, compressionMethod); }
			void setLastModifiedDateTime(const Wt::WDateTime& dateTime) { writeDateTime(10, dateTime); }
			void setCrc32UncompressedData(std::uint32_t crc) { write32(14, crc); }
			void setCompressedSize(std::uint32_t size = UINT32_MAX) { write32(18, size); }
			void setUncompressedSize(std::uint32_t size = UINT32_MAX) { write32(22, size); }
			void setFileNameLength(SizeType size) { write16(26, size); }
			void setExtraFieldLength(SizeType size) { write16(28, size); }
			static constexpr SizeType getHeaderSize() { return 30; }
	};

	class Zip64ExtendedInformationExtraField : public ZipHeader
	{
		public:
			using ZipHeader::ZipHeader;

			struct WithFileOffset {};
			constexpr Zip64ExtendedInformationExtraField(std::byte* buffer, SizeType bufferSize, WithFileOffset)
				: ZipHeader {buffer, bufferSize}
				, _withFileOffset {true}
				{}

			void setTag() { write16(0, 0x0001); }
			void setSize() { write16(2, (_withFileOffset ? getHeaderSize(WithFileOffset {}) : getHeaderSize()) - 4); }
			void setUncompressedSize(SizeType size) { write64(4, size); }
			void setCompressedSize(SizeType size) { write64(12, size); }
			void setFileOffset(SizeType size) { assert(_withFileOffset); write64(20, size); }

			static constexpr SizeType getHeaderSize() { return 20; }
			static constexpr SizeType getHeaderSize(WithFileOffset) { return 28; }

		private:
			const bool _withFileOffset {};
	};

	class DataDescriptor : public ZipHeader
	{
		public:
			using ZipHeader::ZipHeader;

			void setSignature() { write32(0, 0x08074b50 ); }
			void setCrc32UncompressedData(std::uint32_t crc32) { write32(4, crc32); }
			void setCompressedSize(SizeType size) { write64(8, size); }
			void setUncompressedSize(SizeType size) { write64(16, size); }
			static constexpr SizeType getHeaderSize() { return 24; }
	};

	class CentralDirectoryHeader : public ZipHeader
	{
		public:
			using ZipHeader::ZipHeader;

			void setSignature()	{ write32(0, 0x02014b50); }
			void setVersionMadeBy(Version version) { assert(version.minor < 10); write16(4, version.major * 10 + version.minor); }
			void setVersionNeededToExtract(Version version) { assert(version.minor < 10); write16(6, version.major*10 + version.minor); }
			void setGeneralPurposeFlags(std::uint16_t flags) { write16(8, flags); }
			void setCompressionMethod(CompressionMethod method) { write16(10, method); }
			void setLastModifiedDateTime(const Wt::WDateTime& dateTime) { writeDateTime(12, dateTime); }
			void setCrc32UncompressedData(std::uint32_t crc32) { write32(16, crc32); }
			void setCompressedSize(SizeType size = UINT32_MAX) { write32(20, size); }
			void setUncompressedSize(SizeType size = UINT32_MAX) { write32(24, size); }
			void setFileNameLength(SizeType size) { write16(28, size); }
			void setExtraFieldLength(SizeType size) { write16(30, size); }
			void setFileCommentLength(SizeType size) { write16(32, size); }
			void setDiskNumber(SizeType number) { write16(34, number); }
			void setInternalFileAttributes(std::uint16_t attributes) { write16(36, attributes); }
			void setExternalFileAttributes(std::uint16_t attributes) { write32(38, attributes); }
			void setRelativeFileHeaderOffset(SizeType offset = UINT32_MAX) { write32(42, offset); }
			static constexpr SizeType getHeaderSize() { return 46; }
	};

	class Zip64EndOfCentralDirectoryRecord : public ZipHeader
	{
		public:
			using ZipHeader::ZipHeader;

			void setSignature()	{ write32(0, 0x06064b50); }
			void setSize() { write64(4, 56 - 12); }
			void setVersionMadeBy(Version version) { assert(version.minor < 10); write16(12, version.major * 10 + version.minor); }
			void setVersionNeededToExtract(Version version) { assert(version.minor < 10); write16(14, version.major*10 + version.minor); }
			void setDiskNumber(SizeType number) { write32(16, number); }
			void setCentralDirectoryDiskNumber(unsigned number) { write32(20, number); }
			void setNbDiskCentralDirectoryRecords(unsigned number) { write64(24, number); }
			void setNbCentralDirectoryRecords(unsigned number) { write64(32, number); }
			void setCentralDirectorySize(SizeType size) { write64(40, size); }
			void setCentralDirectoryOffset(SizeType offset) { write64(48, offset); }

			static constexpr SizeType getHeaderSize() { return 56; }
	};

	class Zip64EndOfCentralDirectoryLocator : public ZipHeader
	{
		public:
			using ZipHeader::ZipHeader;

			void setSignature() { write32(0, 0x07064b50); }
			void setCentralDirectoryDiskNumber(unsigned number) { write32(4, number); }
			void setZip64EndOfCentralDirectoryOffset(SizeType offset) { write64(8, offset); }
			void setTotalNumberOfDisks(unsigned number) { write32(16, number); };

			static constexpr SizeType getHeaderSize() { return 20; }
	};

	class EndOfCentralDirectoryRecord : public ZipHeader
	{
		public:
			using ZipHeader::ZipHeader;

			void setSignature() { write32(0, 0x06054b50); }
			void setDiskNumber(std::uint16_t number = UINT16_MAX) { write16(4, number); }
			void setCentralDirectoryDiskNumber(std::uint16_t number = UINT16_MAX) { write16(6, number); }
			void setNbDiskCentralDirectoryRecords(std::uint16_t number = UINT16_MAX) { write16(8, number); }
			void setNbCentralDirectoryRecords(std::uint16_t number = UINT16_MAX) { write16(10, number); }
			void setCentralDirectorySize(std::uint32_t size = UINT32_MAX) { write32(12, size); }
			void setCentralDirectoryOffset(std::uint32_t offset = UINT32_MAX) { write32(16, offset); }
			void setCommentLength(SizeType length) { write16(20, length); }
			static constexpr SizeType getHeaderSize() { return 22; }
	};

	Zipper::Zipper(const std::map<std::string, std::filesystem::path>& files, const Wt::WDateTime& lastModifiedTime)
	{
		for (const auto& [filename, filePath] : files)
		{
			FileContext fileContext;
			fileContext.filePath = filePath;

			std::error_code ec;
			fileContext.fileSize = std::filesystem::file_size(filePath, ec);
			if (ec)
				throw ZipperException {"Cannot get file size for '" + filePath.string() + "': " + ec.message()};

			if (lastModifiedTime.isValid())
				fileContext.lastModifiedTime = lastModifiedTime;
			else
				fileContext.lastModifiedTime = PathUtils::getLastWriteTime(filePath);

			_files[filename] = std::move(fileContext);

			_totalZipSize += LocalFileHeader::getHeaderSize();
			_totalZipSize += filename.size();
			_totalZipSize += Zip64ExtendedInformationExtraField::getHeaderSize();
			_totalZipSize += fileContext.fileSize;
			_totalZipSize += DataDescriptor::getHeaderSize();
			_totalZipSize += CentralDirectoryHeader::getHeaderSize();
			_totalZipSize += filename.size();
			_totalZipSize += Zip64ExtendedInformationExtraField::getHeaderSize(Zip64ExtendedInformationExtraField::WithFileOffset {});
		}

		_totalZipSize += Zip64EndOfCentralDirectoryRecord::getHeaderSize();
		_totalZipSize += Zip64EndOfCentralDirectoryLocator::getHeaderSize();
		_totalZipSize += EndOfCentralDirectoryRecord::getHeaderSize();

		_currentFile = std::begin(_files);
	}

	SizeType
	Zipper::writeSome(std::byte* buffer, SizeType bufferSize)
	{
		// make sure we have some room for the headers
		assert(bufferSize >= minOutputBufferSize);

		SizeType nbTotalWrittenBytes {};

		while (!isComplete() && (bufferSize >= minOutputBufferSize))
		{
			SizeType nbWrittenBytes {};

			switch (_writeState)
			{
				case WriteState::LocalFileHeader:
					nbWrittenBytes = writeLocalFileHeader(buffer, bufferSize);
					break;

				case WriteState::LocalFileHeaderFileName:
					nbWrittenBytes = writeLocalFileHeaderFileName(buffer, bufferSize);
					break;

				case WriteState::LocalFileHeaderExtraFields:
					nbWrittenBytes = writeLocalFileHeaderExtraFields(buffer, bufferSize);
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

				case WriteState::CentralDirectoryHeaderExtraFields:
					nbWrittenBytes = writeCentralDirectoryHeaderExtraFields(buffer, bufferSize);
					break;

				case WriteState::Zip64EndOfCentralDirectoryRecord:
					nbWrittenBytes = writeZip64EndOfCentralDirectoryRecord(buffer, bufferSize);
					break;

				case WriteState::Zip64EndOfCentralDirectoryLocator:
					nbWrittenBytes = writeZip64EndOfCentralDirectoryLocator(buffer, bufferSize);
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

	SizeType
	Zipper::writeLocalFileHeader(std::byte* buffer, SizeType bufferSize)
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
		header.setVersionNeededToExtract(ZipHeader::VersionNeededToExtract);
		header.setGeneralPurposeFlags(ZipHeader::GeneralPurposeFlag::LanguageEncoding | ZipHeader::GeneralPurposeFlag::UseDataDescriptor);
		header.setCompressionMethod(ZipHeader::CompressionMethod::NoCompression);
		header.setCrc32UncompressedData(ZipHeader::UnknownCrc32);
		header.setCompressedSize();
		header.setUncompressedSize();
		header.setLastModifiedDateTime(_currentFile->second.lastModifiedTime);
		header.setFileNameLength(_currentFile->first.size());
		header.setExtraFieldLength(Zip64ExtendedInformationExtraField::getHeaderSize());

		_writeState = WriteState::LocalFileHeaderFileName;
		_currentFile->second.localFileHeaderOffset = _currentZipOffset;

		return header.getHeaderSize();
	}

	SizeType
	Zipper::writeLocalFileHeaderFileName(std::byte* buffer, SizeType bufferSize)
	{
		assert(_currentFile != std::end(_files));

		const std::string& fileName {_currentFile->first};

		assert(_currentOffset <= fileName.size());
		if (_currentOffset == fileName.size())
		{
			_writeState = WriteState::LocalFileHeaderExtraFields;
			_currentOffset = 0;
			return 0;
		}

		const SizeType nbBytesToCopy {std::min<std::size_t>(fileName.size() - _currentOffset, bufferSize)};

		std::copy(std::next(std::begin(fileName), _currentOffset), std::next(std::begin(fileName), _currentOffset + nbBytesToCopy), reinterpret_cast<unsigned char*>(buffer));

		_currentOffset += nbBytesToCopy;
		return nbBytesToCopy;
	}

	SizeType
	Zipper::writeLocalFileHeaderExtraFields(std::byte* buffer, SizeType bufferSize)
	{
		assert(_currentFile != std::end(_files));
		static_assert(Zip64ExtendedInformationExtraField::getHeaderSize() <= minOutputBufferSize);

		Zip64ExtendedInformationExtraField header {buffer, bufferSize};

		header.setTag();
		header.setSize();
		header.setUncompressedSize(ZipHeader::UnknownFileSize);
		header.setCompressedSize(ZipHeader::UnknownFileSize);

		_writeState = WriteState::FileData;
		return header.getHeaderSize();
	}

	SizeType
	Zipper::writeFileData(std::byte* buffer, SizeType bufferSize)
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

		const SizeType nbBytesToRead {std::min(static_cast<std::size_t>(fileSize) - _currentOffset, bufferSize)};

		ifs.seekg(_currentOffset, std::ios::beg);
		ifs.read(reinterpret_cast<char*>(buffer), nbBytesToRead );
		const ::uint64_t actualReadSize {static_cast<::uint64_t>(ifs.gcount())};

		_currentFile->second.fileCrc32.processBytes(buffer, actualReadSize);
		_currentOffset += actualReadSize;

		return actualReadSize;
	}

	SizeType
	Zipper::writeDataDescriptor(std::byte* buffer, SizeType bufferSize)
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

	SizeType
	Zipper::writeCentralDirectoryHeader(std::byte* buffer, SizeType bufferSize)
	{
		assert(bufferSize >= minOutputBufferSize);
		static_assert(CentralDirectoryHeader::getHeaderSize() <= minOutputBufferSize);

		if (_currentFile == std::begin(_files))
			_centralDirectoryOffset = _currentZipOffset;

		if (_currentFile == std::end(_files))
		{
			_writeState = WriteState::Zip64EndOfCentralDirectoryRecord;
			_currentFile = std::begin(_files);
			return 0;
		}

		CentralDirectoryHeader header {buffer, bufferSize};
		header.setSignature();
		header.setVersionMadeBy(ZipHeader::VersionMadeBy);
		header.setVersionNeededToExtract(ZipHeader::VersionNeededToExtract);
		header.setGeneralPurposeFlags(ZipHeader::GeneralPurposeFlag::LanguageEncoding | ZipHeader::GeneralPurposeFlag::UseDataDescriptor);
		header.setCompressionMethod(ZipHeader::CompressionMethod::NoCompression);
		header.setCompressedSize();
		header.setUncompressedSize();
		header.setLastModifiedDateTime(_currentFile->second.lastModifiedTime);
		header.setCrc32UncompressedData(_currentFile->second.fileCrc32.getResult());
		header.setFileNameLength(_currentFile->first.size());
		header.setExtraFieldLength(Zip64ExtendedInformationExtraField::getHeaderSize(Zip64ExtendedInformationExtraField::WithFileOffset {}));
		header.setFileCommentLength(0);
		header.setDiskNumber(0);
		header.setInternalFileAttributes(0);
		header.setExternalFileAttributes(0);
		header.setRelativeFileHeaderOffset();

		_writeState = WriteState::CentralDirectoryHeaderFileName;
		_centralDirectorySize += header.getHeaderSize();

		return header.getHeaderSize();
	}

	SizeType
	Zipper::writeCentralDirectoryHeaderFileName(std::byte* buffer, SizeType bufferSize)
	{
		const std::string& fileName {_currentFile->first};

		assert(_currentOffset <= fileName.size());
		if (_currentOffset == fileName.size())
		{
			_currentOffset = 0;
			_writeState = WriteState::CentralDirectoryHeaderExtraFields;

			return 0;
		}

		const SizeType nbBytesToCopy {std::min<std::size_t>(fileName.size() - _currentOffset, bufferSize)};

		std::copy(std::next(std::begin(fileName), _currentOffset), std::next(std::begin(fileName), _currentOffset + nbBytesToCopy), reinterpret_cast<unsigned char*>(buffer));

		_currentOffset += nbBytesToCopy;
		_centralDirectorySize += nbBytesToCopy;
		return nbBytesToCopy;
	}

	SizeType
	Zipper::writeCentralDirectoryHeaderExtraFields(std::byte* buffer, SizeType bufferSize)
	{
		assert(bufferSize >= minOutputBufferSize);
		assert(_currentFile != std::cend(_files));
		static_assert(Zip64ExtendedInformationExtraField::getHeaderSize(Zip64ExtendedInformationExtraField::WithFileOffset {}) <= minOutputBufferSize);

		Zip64ExtendedInformationExtraField header {buffer, bufferSize, Zip64ExtendedInformationExtraField::WithFileOffset {}};

		header.setTag();
		header.setSize();
		header.setUncompressedSize(_currentFile->second.fileSize);
		header.setCompressedSize(_currentFile->second.fileSize);
		header.setFileOffset(_currentFile->second.localFileHeaderOffset);

		++_currentFile;
		_writeState  = WriteState::CentralDirectoryHeader;
		_centralDirectorySize += header.getHeaderSize(Zip64ExtendedInformationExtraField::WithFileOffset {});

		return header.getHeaderSize(Zip64ExtendedInformationExtraField::WithFileOffset {});
	}

	SizeType
	Zipper::writeZip64EndOfCentralDirectoryRecord(std::byte* buffer, SizeType bufferSize)
	{
		assert(bufferSize >= minOutputBufferSize);
		static_assert(Zip64EndOfCentralDirectoryRecord::getHeaderSize() <= minOutputBufferSize);

		Zip64EndOfCentralDirectoryRecord record {buffer, bufferSize};

		record.setSignature();
		record.setSize();
		record.setVersionMadeBy(ZipHeader::VersionNeededToExtract);
		record.setVersionNeededToExtract(ZipHeader::VersionNeededToExtract);
		record.setDiskNumber(0);
		record.setCentralDirectoryDiskNumber(0);
		record.setNbDiskCentralDirectoryRecords(_files.size());
		record.setNbCentralDirectoryRecords(_files.size());
		record.setCentralDirectorySize(_centralDirectorySize);
		record.setCentralDirectoryOffset(_centralDirectoryOffset);

		_zip64EndOfCentralDirectoryRecordOffset = _currentZipOffset;
		_writeState = WriteState::Zip64EndOfCentralDirectoryLocator;
		return record.getHeaderSize();
	}

	SizeType
	Zipper::writeZip64EndOfCentralDirectoryLocator(std::byte* buffer, SizeType bufferSize)
	{
		assert(bufferSize >= minOutputBufferSize);
		static_assert(Zip64EndOfCentralDirectoryLocator::getHeaderSize() <= minOutputBufferSize);

		Zip64EndOfCentralDirectoryLocator locator {buffer, bufferSize};

		locator.setSignature();
		locator.setCentralDirectoryDiskNumber(0);
		locator.setZip64EndOfCentralDirectoryOffset(_zip64EndOfCentralDirectoryRecordOffset);
		locator.setTotalNumberOfDisks(1);

		_writeState = WriteState::EndOfCentralDirectoryRecord;
		return locator.getHeaderSize();
	}

	SizeType
	Zipper::writeEndOfCentralDirectoryRecord(std::byte* buffer, SizeType bufferSize)
	{
		assert(bufferSize >= minOutputBufferSize);
		static_assert(EndOfCentralDirectoryRecord::getHeaderSize() <= minOutputBufferSize);

		EndOfCentralDirectoryRecord record {buffer, bufferSize};

		record.setSignature();
		record.setDiskNumber(0);
		record.setCentralDirectoryDiskNumber(0);
		record.setNbDiskCentralDirectoryRecords();
		record.setNbCentralDirectoryRecords();
		record.setCentralDirectorySize();
		record.setCentralDirectoryOffset();
		record.setCommentLength(0);

		_writeState = WriteState::Complete;
		return record.getHeaderSize();
	}

} // namespace Zip
