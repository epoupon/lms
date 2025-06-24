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

#include "ArchiveZipper.hpp"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <system_error>

#include <archive.h>
#include <archive_entry.h>

#include "core/ILogger.hpp"

namespace lms::zip
{
    std::unique_ptr<IZipper> createArchiveZipper(const EntryContainer& entries)
    {
        return std::make_unique<ArchiveZipper>(entries);
    }

    class FileException : public Exception
    {
    public:
        FileException(const std::filesystem::path& p, std::string_view message)
            : Exception{ "File '" + p.string() + "': " + std::string{ message } }
        {
        }
    };

    class FileStdException : public FileException
    {
    public:
        FileStdException(const std::filesystem::path& p, std::string_view message, int err)
            : FileException{ p, std::string{ message } + "': " + std::error_code{ err, std::generic_category() }.message() }
        {
        }
    };

    class ArchiveException : public Exception
    {
    public:
        ArchiveException(struct ::archive* arch)
            : Exception{ getError(arch) }
        {
        }

        static std::string_view getError(struct ::archive* arch)
        {
            const char* str{ archive_error_string(arch) };
            if (!str)
            {
                static std::string unknownError{ "Unknown archive error" };
                return unknownError;
            }

            return str;
        }
    };

    void ArchiveZipper::ArchiveDeleter::operator()(struct ::archive* arch)
    {
        const int res{ ::archive_write_free(arch) };
        if (res != ARCHIVE_OK)
            LMS_LOG(UTILS, ERROR, "Failure while freeing archive control struct: error code = " << res);
    }

    void ArchiveZipper::ArchiveEntryDeleter::operator()(struct ::archive_entry* archEntry)
    {
        ::archive_entry_free(archEntry);
    }

    ArchiveZipper::ArchiveZipper(const EntryContainer& entries)
        : _entries{ entries }
        , _readBuffer(_readBufferSize, {})
        , _currentEntry{ std::cbegin(_entries) }
    {
        _archive = ArchivePtr{ ::archive_write_new() };
        if (!_archive)
            throw Exception{ "Cannot create archive control struct" };

        auto archiveOpen{ [](struct ::archive*, void*) {
            return ARCHIVE_OK;
        } };

        auto archiveWrite{ [](struct ::archive*, void* clientData, const void* buff, ::size_t n) -> la_ssize_t {
            ArchiveZipper* zipper{ static_cast<ArchiveZipper*>(clientData) };
            return zipper->onWriteCallback(static_cast<const std::byte*>(buff), n);
        } };

        auto archiveClose{ [](struct ::archive*, void*) {
            return ARCHIVE_OK;
        } };

        if (::archive_write_set_bytes_per_block(_archive.get(), _writeBlockSize) != ARCHIVE_OK)
            throw ArchiveException{ _archive.get() };

        // 1 => no padding for last block
        if (::archive_write_set_bytes_in_last_block(_archive.get(), 1) != ARCHIVE_OK)
            throw ArchiveException{ _archive.get() };

        if (::archive_write_set_format_zip(_archive.get()) != ARCHIVE_OK)
            throw ArchiveException{ _archive.get() };

        if (::archive_write_set_option(_archive.get(), "zip", "compression", "deflate") != ARCHIVE_OK)
            throw ArchiveException{ _archive.get() };

        int res{ ::archive_write_open(_archive.get(), this, archiveOpen, archiveWrite, archiveClose) };
        if (res != ARCHIVE_OK)
            throw ArchiveException{ _archive.get() };
    }

    std::uint64_t ArchiveZipper::writeSome(std::ostream& output)
    {
        assert(!_currentOutputStream);

        _currentOutputStream = &output;
        _bytesWrittenInCurrentOutputStream = 0;

        while (_bytesWrittenInCurrentOutputStream == 0)
        {
            if (!_currentArchiveEntry)
            {
                if (_currentEntry == std::cend(_entries))
                {
                    if (::archive_write_close(_archive.get()) != ARCHIVE_OK)
                        throw ArchiveException{ _archive.get() };

                    _archive.reset();
                    break;
                }

                _currentArchiveEntry = createArchiveEntry(*_currentEntry);
                _currentEntryOffset = 0;
                if (::archive_write_header(_archive.get(), _currentArchiveEntry.get()) != ARCHIVE_OK)
                    throw ArchiveException{ _archive.get() };
            }

            if (writeSomeCurrentFileData())
            {
                // entry complete
                if (::archive_write_finish_entry(_archive.get()) != ARCHIVE_OK)
                    throw ArchiveException{ _archive.get() };

                _currentArchiveEntry.reset();
                _currentEntry++;
            }
        }

        _currentOutputStream = nullptr;
        return _bytesWrittenInCurrentOutputStream;
    }

    bool ArchiveZipper::isComplete() const
    {
        return !_archive;
    }

    void ArchiveZipper::abort()
    {
        LMS_LOG(UTILS, DEBUG, "Aborting zip creation");
        if (_archive)
        {
            ::archive_write_fail(_archive.get());
            _archive.reset();
        }
    }

    static ::mode_t permsToMode(const std::filesystem::perms p)
    {
        using std::filesystem::perms;
        ::mode_t mode{};

        auto testPerm{ [](perms p, perms permToTest) {
            return (p & permToTest) == permToTest;
        } };

        if (testPerm(p, perms::owner_read))
            mode |= S_IRUSR;
        if (testPerm(p, perms::owner_write))
            mode |= S_IWUSR;
        if (testPerm(p, perms::owner_exec))
            mode |= S_IXUSR;
        if (testPerm(p, perms::group_read))
            mode |= S_IRGRP;
        if (testPerm(p, perms::group_write))
            mode |= S_IWGRP;
        if (testPerm(p, perms::group_exec))
            mode |= S_IXGRP;
        if (testPerm(p, perms::others_read))
            mode |= S_IROTH;
        if (testPerm(p, perms::others_write))
            mode |= S_IWOTH;
        if (testPerm(p, perms::others_exec))
            mode |= S_IXOTH;

        return mode;
    }

    ArchiveZipper::ArchiveEntryPtr ArchiveZipper::createArchiveEntry(const Entry& entry)
    {
        try
        {
            if (!std::filesystem::is_regular_file(entry.filePath))
                throw FileException{ entry.filePath, "not a regular file" };

            ArchiveEntryPtr archiveEntry{ ::archive_entry_new() };
            if (!archiveEntry)
                throw Exception{ "Cannot create archive entry control struct" };

            ::archive_entry_set_pathname(archiveEntry.get(), entry.fileName.c_str());
            ::archive_entry_set_size(archiveEntry.get(), std::filesystem::file_size(entry.filePath));
            ::archive_entry_set_mode(archiveEntry.get(), permsToMode(std::filesystem::status(entry.filePath).permissions()));
            ::archive_entry_set_filetype(archiveEntry.get(), AE_IFREG);

            return archiveEntry;
        }
        catch (const std::filesystem::filesystem_error& error)
        {
            throw FileException{ entry.filePath, error.what() };
        }
    }

    bool ArchiveZipper::writeSomeCurrentFileData()
    {
        assert(_currentEntry != std::cend(_entries));

        std::ifstream ifs{ _currentEntry->filePath, std::ios_base::binary };
        if (!ifs)
            throw FileStdException{ _currentEntry->filePath, "cannot open file", errno };

        ifs.seekg(0, std::ios::end);
        const std::uint64_t fileSize{ static_cast<std::uint64_t>(ifs.tellg()) };
        ifs.seekg(0, std::ios::beg);

        // TODO: store file size?
        if (fileSize < _currentEntryOffset)
            throw FileException{ _currentEntry->filePath, "size changed?" };

        const std::uint64_t bytesToRead{ std::min(fileSize - _currentEntryOffset, static_cast<std::uint64_t>(_readBufferSize)) };

        // read from file
        if (!ifs.seekg(_currentEntryOffset, std::ios::beg))
            throw FileStdException{ _currentEntry->filePath, "seek failed", errno };

        if (!ifs.read(reinterpret_cast<char*>(_readBuffer.data()), bytesToRead))
            throw FileStdException{ _currentEntry->filePath, "read failed", errno };

        const std::uint64_t actualBytesRead{ static_cast<std::uint64_t>(ifs.gcount()) };

        // write to archive
        {
            std::uint64_t remainingBytesToWrite{ actualBytesRead };
            while (remainingBytesToWrite > 0)
            {
                const auto writtenBytes{ ::archive_write_data(_archive.get(), &_readBuffer[actualBytesRead - remainingBytesToWrite], remainingBytesToWrite) };
                if (writtenBytes < 0)
                    throw ArchiveException{ _archive.get() };

                assert(static_cast<std::uint64_t>(writtenBytes) <= remainingBytesToWrite);
                remainingBytesToWrite -= writtenBytes;
            }
        }

        _currentEntryOffset += actualBytesRead;
        return (_currentEntryOffset >= fileSize);
    }

    std::int64_t ArchiveZipper::onWriteCallback(const std::byte* buffer, std::size_t bufferSize)
    {
        if (!_currentOutputStream)
        {
            ::archive_set_error(_archive.get(), EIO, "IO error: operation cancelled");
            return -1;
        }

        _currentOutputStream->write(reinterpret_cast<const char*>(buffer), bufferSize);
        if (!*_currentOutputStream)
            throw Exception{ "Failed to write " + std::to_string(bufferSize) + " bytes in final archive output!" };

        _bytesWrittenInCurrentOutputStream += bufferSize;

        return bufferSize;
    }
} // namespace lms::zip
