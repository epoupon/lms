/*
 * Copyright (C) 2019 Emeric Poupon
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

#include <gtest/gtest.h>

#include "core/Path.hpp"

namespace lms::core::pathUtils::tests
{
    TEST(Path, getLongestCommonPath)
    {
        struct TestCase
        {
            std::filesystem::path path1;
            std::filesystem::path path2;
            std::filesystem::path expectedCommonPath;
        };

        TestCase tests[]{
            { "foo.txt", "/foo/foo.txt", "" },
            { "/", "/file.txt", "/" },
            { "/foo/bar/file1.txt", "/foo/bar/file2.txt", "/foo/bar" },
            { "/foo/bar/file.txt", "/foo/bar/file.txt", "/foo/bar/file.txt" },
            { "/dir1/file.txt", "/dir2/file.txt", "/" },
            { "/prefix/folder/file.txt", "/prefix/folder/subfolder/file.txt", "/prefix/folder" },
        };

        for (const TestCase& test : tests)
        {
            EXPECT_EQ(core::pathUtils::getLongestCommonPath(test.path1, test.path2), test.expectedCommonPath);
        }
    }

    TEST(Path, getLongestCommonPathIterator)
    {
        struct TestCase
        {
            std::vector<std::filesystem::path> paths;
            std::filesystem::path expectedCommonPath;
        };

        TestCase tests[]{
            { {}, "" },
            { { "/" }, "/" },
            { { "/foo", "/bar" }, "/" },
            { { "/foo/bar/file1.txt", "/foo/bar/file2.txt" }, "/foo/bar" },
            { { "/foo", "/foo/" }, "/foo" },
            { { "/foo", "/foo" }, "/foo" },
            { { "/foo/", "/foo/" }, "/foo/" },
            { { "/foo/", "/foo/", "/bar" }, "/" },
            { { "/foo/", "/foo/", "/foo/bar" }, "/foo" },
        };

        for (const TestCase& test : tests)
        {
            EXPECT_EQ(core::pathUtils::getLongestCommonPath(std::cbegin(test.paths), std::cend(test.paths)), test.expectedCommonPath);
        }
    }

    TEST(Path, isPathInRootPath)
    {
        struct TestCase
        {
            std::filesystem::path path;
            std::filesystem::path rootPath;
            bool expectedResult;
        };

        TestCase tests[]{
            { "/file.txt", "/", true },
            { "/root/folder/file.txt", "/root", true },
            { "/root/file.txt", "/root", true },
            { "/root/file.txt", "/root/", true },
            { "/root", "/root", true },
            { "/root", "/root/", true },
            { "/root/", "/root", true },
            { "/root/", "/root/", true },
            { "/folder/file.txt", "/root", false },
            { "/folder/file.txt", "/root/", false },
            { "/file.txt", "/root", false },
            { "/file.txt", "/root/", false },
            { "", "/root", false },
        };

        for (const TestCase& test : tests)
        {
            EXPECT_EQ(core::pathUtils::isPathInRootPath(test.path, test.rootPath), test.expectedResult) << "Failed: path = " << test.path << ", rootPath = " << test.rootPath;
        }
    }

    TEST(Path, sanitizeFileStem)
    {
        struct TestCase
        {
            std::string input;
            std::string_view expectedOutput;
        };

        TestCase tests[]{
            { "", "" }, // empty input
            { "valid_file_name", "valid_file_name" },
            { "invalid:file*name?", "invalidfilename" },
            { "another|invalid<name>", "anotherinvalidname" },
            { "/leading/slash", "leadingslash" },
            { "\\backslash\\file", "backslashfile" },
            { "file_with_äöüß", "file_with_äöüß" },                // keep German umlauts
            { "file_with_éèêë", "file_with_éèêë" },                // keep French accents
            { "héllo 漢字", "héllo 漢字" },                        // keep UTF8 characters
            { "file_with_üñîçødë", "file_with_üñîçødë" },          // keep special characters
            { "file_with_!@#$%^&*()_+", "file_with_!@#$%^&()_+" }, // remove special characters
            { "file_with_", "file_with_" },                        // handle double dots
            { "file.with.extension", "file.with.extension" },      // keep extensions
        };

        for (const TestCase& test : tests)
        {
            EXPECT_EQ(core::pathUtils::sanitizeFileStem(test.input), test.expectedOutput) << "Failed: input = " << test.input;
        }
    }
} // namespace lms::core::pathUtils::tests