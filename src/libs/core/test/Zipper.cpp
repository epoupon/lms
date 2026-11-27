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

    } // namespace

    TEST(Zipper, nonAsciiFileName)
    {
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
        ASSERT_EQ(readUInt16LE(zip, 26), fileName.size());
        EXPECT_EQ(zip.substr(30, fileName.size()), fileName);

        // central directory header, the one unzip tools read
        const std::size_t centralHeader{ zip.find("PK\x01\x02") };
        ASSERT_NE(centralHeader, std::string::npos);
        ASSERT_EQ(readUInt16LE(zip, centralHeader + 28), fileName.size());
        EXPECT_EQ(zip.substr(centralHeader + 46, fileName.size()), fileName);
    }

    TEST(Zipper, missingFile)
    {
        const core::ScopedTmpDirectory directory;
        const std::filesystem::path filePath{ directory / "file.txt" };
        std::ofstream{ filePath } << "content";

        std::ostringstream oss;
        {
            const std::unique_ptr<IZipper> zipper{ createArchiveZipper(EntryContainer{ Entry{ .fileName = "file.txt", .filePath = filePath }, Entry{ .fileName = "missing.txt", .filePath = directory / "missing.txt" } }) };
            EXPECT_THROW(
                while (!zipper->isComplete()) {
                    zipper->writeSome(oss);
                },
                Exception);
        }

        // no central directory, even once the zipper is gone: the zip must not look complete
        EXPECT_EQ(oss.str().find("PK\x01\x02"), std::string::npos);
    }
} // namespace lms::zip::tests
