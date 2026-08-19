/*
 * Copyright (C) 2025 Emeric Poupon
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

#include "IgnoreRules.hpp"

namespace lms::scanner::tests
{
    TEST(IgnoreRules, EmptyContent)
    {
        const IgnoreRules f{ "" };
        EXPECT_TRUE(f.isEmpty());
        EXPECT_FALSE(f.isIgnored("track.flac", IgnoreRules::IsDirectory{ false }));
    }

    TEST(IgnoreRules, EmptyPath_NeverIgnored)
    {
        const IgnoreRules f{ "*.mp3\ncovers/\n" };
        EXPECT_FALSE(f.isIgnored("", IgnoreRules::IsDirectory{ false }));
        EXPECT_FALSE(f.isIgnored("", IgnoreRules::IsDirectory{ true }));
    }

    TEST(IgnoreRules, CommentsAndBlanksOnly)
    {
        const IgnoreRules f{ "# this is a comment\n\n# another comment\n" };
        EXPECT_TRUE(f.isEmpty());
    }

    TEST(IgnoreRules, Basename_MatchesAtRoot)
    {
        const IgnoreRules f{ "*.nfo\n" };
        EXPECT_TRUE(f.isIgnored("track.nfo", IgnoreRules::IsDirectory{ false }));
        EXPECT_FALSE(f.isIgnored("track.flac", IgnoreRules::IsDirectory{ false }));
    }

    TEST(IgnoreRules, Basename_MatchesInSubdir)
    {
        const IgnoreRules f{ "*.nfo\n" };
        EXPECT_TRUE(f.isIgnored("jazz/miles/track.nfo", IgnoreRules::IsDirectory{ false }));
        EXPECT_FALSE(f.isIgnored("jazz/miles/track.flac", IgnoreRules::IsDirectory{ false }));
    }

    TEST(IgnoreRules, Basename_QuestionMark)
    {
        const IgnoreRules f{ "?.nfo\n" };
        EXPECT_TRUE(f.isIgnored("a.nfo", IgnoreRules::IsDirectory{ false }));
        EXPECT_FALSE(f.isIgnored("ab.nfo", IgnoreRules::IsDirectory{ false }));
    }

    TEST(IgnoreRules, Basename_ExactName)
    {
        const IgnoreRules f{ "Thumbs.db\n" };
        EXPECT_TRUE(f.isIgnored("Thumbs.db", IgnoreRules::IsDirectory{ false }));
        EXPECT_FALSE(f.isIgnored("thumbs.db", IgnoreRules::IsDirectory{ false })); // case-sensitive
    }

    TEST(IgnoreRules, Basename_MultipleWildcards)
    {
        const IgnoreRules f{ "cover*.*\n" };
        EXPECT_TRUE(f.isIgnored("cover.jpg", IgnoreRules::IsDirectory{ false }));
        EXPECT_TRUE(f.isIgnored("cover_front.jpg", IgnoreRules::IsDirectory{ false }));
        EXPECT_TRUE(f.isIgnored("cover.png", IgnoreRules::IsDirectory{ false }));
        EXPECT_FALSE(f.isIgnored("notcover.jpg", IgnoreRules::IsDirectory{ false }));
        EXPECT_FALSE(f.isIgnored("coverjpg", IgnoreRules::IsDirectory{ false })); // no dot
    }

    TEST(IgnoreRules, Basename_WildcardInDirName)
    {
        const IgnoreRules f{ "covers*/\n" };
        EXPECT_TRUE(f.isIgnored("covers", IgnoreRules::IsDirectory{ true }));
        EXPECT_TRUE(f.isIgnored("covers_2024", IgnoreRules::IsDirectory{ true }));
        EXPECT_TRUE(f.isIgnored("jazz/covers_hq", IgnoreRules::IsDirectory{ true })); // unanchored: matches at any depth
        EXPECT_FALSE(f.isIgnored("notcovers", IgnoreRules::IsDirectory{ true }));
        EXPECT_FALSE(f.isIgnored("covers_2024", IgnoreRules::IsDirectory{ false })); // dirOnly
    }

    TEST(IgnoreRules, FullPath_WildcardInPathComponent)
    {
        const IgnoreRules f{ "jazz/*/liner.nfo\n" };
        EXPECT_TRUE(f.isIgnored("jazz/miles/liner.nfo", IgnoreRules::IsDirectory{ false }));
        EXPECT_TRUE(f.isIgnored("jazz/coltrane/liner.nfo", IgnoreRules::IsDirectory{ false }));
        EXPECT_FALSE(f.isIgnored("jazz/liner.nfo", IgnoreRules::IsDirectory{ false }));             // no intermediate component
        EXPECT_FALSE(f.isIgnored("jazz/miles/davis/liner.nfo", IgnoreRules::IsDirectory{ false })); // * doesn't cross /
        EXPECT_FALSE(f.isIgnored("rock/miles/liner.nfo", IgnoreRules::IsDirectory{ false }));
    }

    TEST(IgnoreRules, Basename_MatchesDirectoryToo)
    {
        // patterns without trailing / apply to both files and directories
        const IgnoreRules f{ "*.nfo\n" };
        EXPECT_TRUE(f.isIgnored("track.nfo", IgnoreRules::IsDirectory{ false }));
        EXPECT_TRUE(f.isIgnored("track.nfo", IgnoreRules::IsDirectory{ true }));
    }

    TEST(IgnoreRules, DirOnly_IgnoresDirectory)
    {
        const IgnoreRules f{ "covers/\n" };
        EXPECT_TRUE(f.isIgnored("covers", IgnoreRules::IsDirectory{ true }));
        EXPECT_FALSE(f.isIgnored("covers", IgnoreRules::IsDirectory{ false })); // not a dir
    }

    TEST(IgnoreRules, DirOnly_AnchoredOnlyMatchesRoot)
    {
        const IgnoreRules f{ "/untagged/\n" };
        EXPECT_TRUE(f.isIgnored("untagged", IgnoreRules::IsDirectory{ true }));       // root level: match
        EXPECT_FALSE(f.isIgnored("jazz/untagged", IgnoreRules::IsDirectory{ true })); // nested: no match
        EXPECT_FALSE(f.isIgnored("untagged", IgnoreRules::IsDirectory{ false }));     // file, not dir: no match
    }

    TEST(IgnoreRules, DirOnly_UnanchoredMatchesAnyDepth)
    {
        const IgnoreRules f{ "covers/\n" };
        EXPECT_TRUE(f.isIgnored("covers", IgnoreRules::IsDirectory{ true }));       // root: match
        EXPECT_TRUE(f.isIgnored("jazz/covers", IgnoreRules::IsDirectory{ true }));  // nested: also match
        EXPECT_TRUE(f.isIgnored("a/b/c/covers", IgnoreRules::IsDirectory{ true })); // deep: also match
        EXPECT_FALSE(f.isIgnored("covers", IgnoreRules::IsDirectory{ false }));     // file, not dir: no match
    }

    TEST(IgnoreRules, FullPath_ExactDir)
    {
        const IgnoreRules f{ "jazz/covers\n" };
        EXPECT_TRUE(f.isIgnored("jazz/covers", IgnoreRules::IsDirectory{ true }));
        EXPECT_TRUE(f.isIgnored("jazz/covers/foo", IgnoreRules::IsDirectory{ true }));  // descendant of an ignored dir
        EXPECT_FALSE(f.isIgnored("foo/jazz/covers", IgnoreRules::IsDirectory{ true })); // unrelated path structure, no prefix matches
        EXPECT_FALSE(f.isIgnored("rock/covers", IgnoreRules::IsDirectory{ true }));
    }

    TEST(IgnoreRules, AncestorDirIgnored_MatchesDescendantFile)
    {
        const IgnoreRules f{ "import\n" };
        EXPECT_TRUE(f.isIgnored("import/song.mp3", IgnoreRules::IsDirectory{ false }));
        EXPECT_TRUE(f.isIgnored("import/sub/song.mp3", IgnoreRules::IsDirectory{ false }));
        EXPECT_FALSE(f.isIgnored("other/song.mp3", IgnoreRules::IsDirectory{ false }));
    }

    TEST(IgnoreRules, AncestorDirIgnored_AnchoredRuleMatchesNestedFile)
    {
        const IgnoreRules f{ "/import/\n" };
        EXPECT_TRUE(f.isIgnored("import/song.mp3", IgnoreRules::IsDirectory{ false }));
        EXPECT_FALSE(f.isIgnored("jazz/import/song.mp3", IgnoreRules::IsDirectory{ false })); // anchored to root only
    }

    TEST(IgnoreRules, AncestorDirIgnored_NegationDoesNotReinclude)
    {
        const IgnoreRules f{ "jazz/\n!jazz/keep.flac\n" };
        EXPECT_TRUE(f.isIgnored("jazz/keep.flac", IgnoreRules::IsDirectory{ false })); // ancestor dir ignored: negation deeper inside has no effect
        EXPECT_TRUE(f.isIgnored("jazz", IgnoreRules::IsDirectory{ true }));
    }

    TEST(IgnoreRules, FullPath_GlobInDir)
    {
        const IgnoreRules f{ "jazz/*.nfo\n" };
        EXPECT_TRUE(f.isIgnored("jazz/liner.nfo", IgnoreRules::IsDirectory{ false }));
        EXPECT_FALSE(f.isIgnored("liner.nfo", IgnoreRules::IsDirectory{ false }));
        EXPECT_FALSE(f.isIgnored("rock/liner.nfo", IgnoreRules::IsDirectory{ false }));
    }

    TEST(IgnoreRules, Negation_ReIncludesAfterBroadMatch)
    {
        const IgnoreRules f{ "*.nfo\n!important.nfo\n" };
        EXPECT_TRUE(f.isIgnored("track.nfo", IgnoreRules::IsDirectory{ false }));
        EXPECT_FALSE(f.isIgnored("important.nfo", IgnoreRules::IsDirectory{ false }));
    }

    TEST(IgnoreRules, Negation_NoEffectIfNoPriorMatch)
    {
        const IgnoreRules f{ "!track.nfo\n" };
        EXPECT_FALSE(f.isIgnored("track.nfo", IgnoreRules::IsDirectory{ false }));
    }

    TEST(IgnoreRules, LastRuleWins)
    {
        const IgnoreRules f{ "*.flac\n!keep.flac\n*.flac\n" };
        EXPECT_TRUE(f.isIgnored("keep.flac", IgnoreRules::IsDirectory{ false }));
    }

    TEST(IgnoreRules, WindowsLineEndings)
    {
        const IgnoreRules f{ "*.nfo\r\n*.jpg\r\n" };
        EXPECT_TRUE(f.isIgnored("track.nfo", IgnoreRules::IsDirectory{ false }));
        EXPECT_TRUE(f.isIgnored("cover.jpg", IgnoreRules::IsDirectory{ false }));
        EXPECT_FALSE(f.isIgnored("track.flac", IgnoreRules::IsDirectory{ false }));
    }
} // namespace lms::scanner::tests
