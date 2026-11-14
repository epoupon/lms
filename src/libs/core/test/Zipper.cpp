/*
 * Copyright (C) 2026 Emeric Poupon
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

#include <clocale>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "core/IZipper.hpp"
#include "core/ScopedTmpDirectory.hpp"

namespace lms::zip::tests
{
    namespace
    {
        std::uint16_t readUInt16LE(std::string_view data, std::size_t offset)
        {
            return static_cast<std::uint16_t>(static_cast<unsigned char>(data[offset]) | (static_cast<unsigned char>(data[offset + 1]) << 8));
        }

        constexpr std::uint16_t utf8NameFlag{ 0x0800 };
    } // namespace

    TEST(Zipper, utf8FileName)
    {
        // A process stays in the "C" locale until it calls setlocale: names must not depend on it
        ASSERT_STREQ(std::setlocale(LC_ALL, nullptr), "C");

        const core::ScopedTmpDirectory directory;
        const std::filesystem::path filePath{ directory / "file.txt" };
        std::ofstream{ filePath } << "content";

        const std::string fileName{ "\xC3\x89t\xC3\xA9.txt" };
        const std::unique_ptr<IZipper> zipper{ createArchiveZipper(EntryContainer{ Entry{ .fileName = fileName, .filePath = filePath } }) };

        std::ostringstream oss;
        while (!zipper->isComplete())
            zipper->writeSome(oss);
        const std::string zip{ oss.str() };

        // local file header
        ASSERT_EQ(zip.substr(0, 4), "PK\x03\x04");
        EXPECT_EQ(readUInt16LE(zip, 6) & utf8NameFlag, utf8NameFlag);
        ASSERT_EQ(readUInt16LE(zip, 26), fileName.size());
        EXPECT_EQ(zip.substr(30, fileName.size()), fileName);

        // central directory header, the one unzip tools read
        const std::size_t centralHeader{ zip.find("PK\x01\x02") };
        ASSERT_NE(centralHeader, std::string::npos);
        EXPECT_EQ(readUInt16LE(zip, centralHeader + 8) & utf8NameFlag, utf8NameFlag);
        ASSERT_EQ(readUInt16LE(zip, centralHeader + 28), fileName.size());
        EXPECT_EQ(zip.substr(centralHeader + 46, fileName.size()), fileName);
    }

    TEST(Zipper, invalidUtf8FileName)
    {
        const core::ScopedTmpDirectory directory;
        const std::filesystem::path filePath{ directory / "file.txt" };
        std::ofstream{ filePath } << "content";

        const std::string fileName{ "Caf\xE9.txt" };
        const std::unique_ptr<IZipper> zipper{ createArchiveZipper(EntryContainer{ Entry{ .fileName = fileName, .filePath = filePath } }) };

        std::ostringstream oss;
        while (!zipper->isComplete())
            zipper->writeSome(oss);
        const std::string zip{ oss.str() };

        // libarchive keeps the original bytes when the name cannot be checked as UTF-8
        EXPECT_EQ(zip.substr(30, readUInt16LE(zip, 26)), fileName);
    }
} // namespace lms::zip::tests
