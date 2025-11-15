/*
 * Copyright (C) 2024 Emeric Poupon
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

#include <vector>

#include <gtest/gtest.h>

#include <Wt/WTime.h>

#include "scanners/audiofile/TrackMetadataParser.hpp"

#include "TestTagReader.hpp"

namespace lms::scanner::tests
{
    TEST(TrackMetadataParser, generalTest)
    {
        TrackMetadataParser::Parameters params;
        params.userExtraTags = { "MY_AWESOME_TAG_A", "MY_AWESOME_TAG_B", "MY_AWESOME_MISSING_TAG" };

        TrackMetadataParser parser{ params };
        std::unique_ptr<audio::ITagReader> testTags{ createDefaultPopulatedTestTagReader() };

        const Track track{ parser.parseTrackMetaData(*testTags) };

        EXPECT_EQ(track.acoustID, core::UUID::fromString("e987a441-e134-4960-8019-274eddacc418"));
        ASSERT_TRUE(track.advisory.has_value());
        EXPECT_EQ(track.advisory.value(), Track::Advisory::Clean);
        EXPECT_EQ(track.artistDisplayName, "MyArtist1 & MyArtist2");
        ASSERT_EQ(track.artists.size(), 2);
        EXPECT_EQ(track.artists[0].name, "MyArtist1");
        EXPECT_EQ(track.artists[0].sortName, "MyArtists1SortName");
        EXPECT_EQ(track.artists[0].mbid, core::UUID::fromString("9d2e0c8c-8c5e-4372-a061-590955eaeaae"));
        EXPECT_EQ(track.artists[1].name, "MyArtist2");
        EXPECT_EQ(track.artists[1].sortName, "MyArtists2SortName");
        EXPECT_EQ(track.artists[1].mbid, core::UUID::fromString("5e2cf87f-c8d7-4504-8a86-954dc0840229"));
        ASSERT_EQ(track.comments.size(), 2);
        EXPECT_EQ(track.comments[0], "Comment1");
        EXPECT_EQ(track.comments[1], "Comment2");
        ASSERT_EQ(track.composerArtists.size(), 2);
        EXPECT_EQ(track.composerArtists[0].name, "MyComposer1");
        EXPECT_EQ(track.composerArtists[0].sortName, "MyComposerSortOrder1");
        EXPECT_EQ(track.composerArtists[1].name, "MyComposer2");
        EXPECT_EQ(track.composerArtists[1].sortName, "MyComposerSortOrder2");
        ASSERT_EQ(track.conductorArtists.size(), 2);
        EXPECT_EQ(track.conductorArtists[0].name, "MyConductor1");
        EXPECT_EQ(track.conductorArtists[1].name, "MyConductor2");
        EXPECT_EQ(track.copyright, "MyCopyright");
        EXPECT_EQ(track.copyrightURL, "MyCopyrightURL");
        ASSERT_TRUE(track.date.isValid());
        EXPECT_EQ(track.date.getYear(), 2020);
        EXPECT_EQ(track.date.getMonth(), 3);
        EXPECT_EQ(track.date.getDay(), 4);
        ASSERT_EQ(track.genres.size(), 2);
        EXPECT_EQ(track.genres[0], "Genre1");
        EXPECT_EQ(track.genres[1], "Genre2");
        ASSERT_EQ(track.groupings.size(), 2);
        EXPECT_EQ(track.groupings[0], "Grouping1");
        EXPECT_EQ(track.groupings[1], "Grouping2");
        ASSERT_EQ(track.languages.size(), 2);
        EXPECT_EQ(track.languages[0], "Language1");
        EXPECT_EQ(track.languages[1], "Language2");
        ASSERT_EQ(track.lyricistArtists.size(), 2);
        EXPECT_EQ(track.lyricistArtists[0].name, "MyLyricist1");
        EXPECT_EQ(track.lyricistArtists[1].name, "MyLyricist2");
        ASSERT_EQ(track.lyrics.size(), 1);
        EXPECT_EQ(track.lyrics.front().language, "eng");
        ASSERT_EQ(track.lyrics.front().synchronizedLines.size(), 2);
        ASSERT_TRUE(track.lyrics.front().synchronizedLines.contains(std::chrono::milliseconds{ 0 }));
        EXPECT_EQ(track.lyrics.front().synchronizedLines.find(std::chrono::milliseconds{ 0 })->second, "First line");
        ASSERT_TRUE(track.lyrics.front().synchronizedLines.contains(std::chrono::milliseconds{ 1000 }));
        EXPECT_EQ(track.lyrics.front().synchronizedLines.find(std::chrono::milliseconds{ 1000 })->second, "Second line");
        ASSERT_TRUE(track.mbid.has_value());
        EXPECT_EQ(track.mbid.value(), core::UUID::fromString("0afb190a-6735-46df-a16d-199f48206e4a"));
        ASSERT_EQ(track.mixerArtists.size(), 2);
        EXPECT_EQ(track.mixerArtists[0].name, "MyMixer1");
        EXPECT_EQ(track.mixerArtists[1].name, "MyMixer2");
        ASSERT_EQ(track.moods.size(), 2);
        EXPECT_EQ(track.moods[0], "Mood1");
        EXPECT_EQ(track.moods[1], "Mood2");
        ASSERT_TRUE(track.originalDate.isValid());
        EXPECT_EQ(track.originalDate.getYear(), 2019);
        EXPECT_EQ(track.originalDate.getMonth(), 2);
        EXPECT_EQ(track.originalDate.getDay(), 3);
        ASSERT_TRUE(track.originalYear.has_value());
        EXPECT_EQ(track.originalYear.value(), 2019);
        ASSERT_TRUE(track.performerArtists.contains("Rolea"));
        ASSERT_EQ(track.performerArtists.at("Rolea").size(), 2);
        EXPECT_EQ(track.performerArtists.at("Rolea")[0].name, "MyPerformer1ForRoleA");
        EXPECT_EQ(track.performerArtists.at("Rolea")[1].name, "MyPerformer2ForRoleA");
        ASSERT_EQ(track.performerArtists.at("Roleb").size(), 2);
        EXPECT_EQ(track.performerArtists.at("Roleb")[0].name, "MyPerformer1ForRoleB");
        EXPECT_EQ(track.performerArtists.at("Roleb")[1].name, "MyPerformer2ForRoleB");
        ASSERT_TRUE(track.position.has_value());
        EXPECT_EQ(track.position.value(), 7);
        ASSERT_EQ(track.producerArtists.size(), 2);
        EXPECT_EQ(track.producerArtists[0].name, "MyProducer1");
        EXPECT_EQ(track.producerArtists[1].name, "MyProducer2");
        ASSERT_TRUE(track.recordingMBID.has_value());
        EXPECT_EQ(track.recordingMBID.value(), core::UUID::fromString("bd3fc666-89de-4ac8-93f6-2dbf028ad8d5"));
        ASSERT_TRUE(track.replayGain.has_value());
        EXPECT_FLOAT_EQ(track.replayGain.value(), -0.33);
        ASSERT_EQ(track.remixerArtists.size(), 2);
        EXPECT_EQ(track.remixerArtists[0].name, "MyRemixer1");
        EXPECT_EQ(track.remixerArtists[1].name, "MyRemixer2");
        EXPECT_EQ(track.title, "MyTitle");
        ASSERT_EQ(track.userExtraTags.at("MY_AWESOME_TAG_A").size(), 2);
        EXPECT_EQ(track.userExtraTags.at("MY_AWESOME_TAG_A")[0], "MyTagValue1ForTagA");
        EXPECT_EQ(track.userExtraTags.at("MY_AWESOME_TAG_A")[1], "MyTagValue2ForTagA");
        ASSERT_EQ(track.userExtraTags.at("MY_AWESOME_TAG_B").size(), 2);
        EXPECT_EQ(track.userExtraTags.at("MY_AWESOME_TAG_B")[0], "MyTagValue1ForTagB");
        EXPECT_EQ(track.userExtraTags.at("MY_AWESOME_TAG_B")[1], "MyTagValue2ForTagB");

        // Medium
        ASSERT_TRUE(track.medium.has_value());
        EXPECT_EQ(track.medium->media, "CD");
        EXPECT_EQ(track.medium->name, "MySubtitle");
        ASSERT_TRUE(track.medium->position.has_value());
        EXPECT_EQ(track.medium->position.value(), 2);
        ASSERT_TRUE(track.medium->replayGain.has_value());
        EXPECT_FLOAT_EQ(track.medium->replayGain.value(), -0.5);
        ASSERT_TRUE(track.medium->trackCount.has_value());
        EXPECT_EQ(track.medium->trackCount.value(), 12);

        // Release
        ASSERT_TRUE(track.medium->release.has_value());
        const Release& release{ track.medium->release.value() };
        EXPECT_EQ(release.artistDisplayName, "MyAlbumArtist1 & MyAlbumArtist2");
        ASSERT_EQ(release.artists.size(), 2);
        EXPECT_EQ(release.artists[0].name, "MyAlbumArtist1");
        EXPECT_EQ(release.artists[0].sortName, "MyAlbumArtists1SortName");
        EXPECT_EQ(release.artists[0].mbid, core::UUID::fromString("6fbf097c-1487-43e8-874b-50dd074398a7"));
        EXPECT_EQ(release.artists[1].name, "MyAlbumArtist2");
        EXPECT_EQ(release.artists[1].sortName, "MyAlbumArtists2SortName");
        EXPECT_EQ(release.artists[1].mbid, core::UUID::fromString("5ed3d6b3-2aed-4a03-828c-3c4d4f7406e1"));
        EXPECT_TRUE(release.isCompilation);
        EXPECT_EQ(release.barcode, "MyBarcode");
        ASSERT_EQ(release.labels.size(), 2);
        EXPECT_EQ(release.labels[0], "Label1");
        EXPECT_EQ(release.labels[1], "Label2");
        ASSERT_TRUE(release.mbid.has_value());
        EXPECT_EQ(release.mbid.value(), core::UUID::fromString("3fa39992-b786-4585-a70e-85d5cc15ef69"));
        EXPECT_EQ(release.groupMBID.value(), core::UUID::fromString("5b1a5a44-8420-4426-9b86-d25dc8d04838"));
        EXPECT_EQ(release.mediumCount, 3);
        EXPECT_EQ(release.name, "MyAlbum");
        EXPECT_EQ(release.sortName, "MyAlbumSortName");
        EXPECT_EQ(release.comment, "MyAlbumComment");
        ASSERT_EQ(release.countries.size(), 2);
        EXPECT_EQ(release.countries[0], "MyCountry1");
        EXPECT_EQ(release.countries[1], "MyCountry2");
        {
            std::vector<std::string> expectedReleaseTypes{ "Album", "Compilation" };
            EXPECT_EQ(release.releaseTypes, expectedReleaseTypes);
        }
    }

    TEST(TrackMetadataParser, trim)
    {
        const TestTagReader testTags{
            {
                { audio::TagType::Genre, { "Genre1 ", " Genre2", " Genre3 " } },
            }
        };

        const Track track{ TrackMetadataParser{}.parseTrackMetaData(testTags) };

        ASSERT_EQ(track.genres.size(), 3);
        EXPECT_EQ(track.genres[0], "Genre1");
        EXPECT_EQ(track.genres[1], "Genre2");
        EXPECT_EQ(track.genres[2], "Genre3");
    }

    TEST(TrackMetadataParser, customDelimiters)
    {
        const TestTagReader testTags{
            {
                { audio::TagType::Album, { "MyAlbum" } },
                { audio::TagType::AlbumArtist, { "AlbumArtist1 /  AlbumArtist2" } },
                { audio::TagType::Artist, { " Artist1 / Artist2 feat. Artist3  " } },
                { audio::TagType::Genre, { "Genre1 ;  Genre2" } },
                { audio::TagType::Language, { " Lang1/Lang2 / Lang3" } },
            }
        };

        TrackMetadataParser::Parameters params;
        params.defaultTagDelimiters = { " ; ", "/" };
        params.artistTagDelimiters = { " / ", " feat. " };
        TrackMetadataParser parser{ params };
        const Track track{ parser.parseTrackMetaData(testTags) };

        ASSERT_EQ(track.artists.size(), 3);
        EXPECT_EQ(track.artists[0].name, "Artist1");
        EXPECT_EQ(track.artists[1].name, "Artist2");
        EXPECT_EQ(track.artists[2].name, "Artist3");
        EXPECT_EQ(track.artistDisplayName, "Artist1, Artist2, Artist3"); // reconstruct artist display name since a custom delimiter is hit
        ASSERT_EQ(track.genres.size(), 2);
        EXPECT_EQ(track.genres[0], "Genre1");
        EXPECT_EQ(track.genres[1], "Genre2");
        ASSERT_EQ(track.languages.size(), 3);
        EXPECT_EQ(track.languages[0], "Lang1");
        EXPECT_EQ(track.languages[1], "Lang2");
        EXPECT_EQ(track.languages[2], "Lang3");

        // Medium
        ASSERT_TRUE(track.medium.has_value());

        // Release
        ASSERT_TRUE(track.medium->release.has_value());
        EXPECT_EQ(track.medium->release->name, "MyAlbum");
        ASSERT_EQ(track.medium->release->artists.size(), 2);
        EXPECT_EQ(track.medium->release->artists[0].name, "AlbumArtist1");
        EXPECT_EQ(track.medium->release->artists[1].name, "AlbumArtist2");
        EXPECT_EQ(track.medium->release->artistDisplayName, "AlbumArtist1, AlbumArtist2");
    }

    TEST(TrackMetadataParser, customArtistDelimiters_whitelist)
    {
        const TestTagReader testTags{
            {
                { audio::TagType::Album, { "MyAlbum" } },
                { audio::TagType::AlbumArtist, { "  AC/DC " } },
                { audio::TagType::Artist, { "AC/DC  " } },
            }
        };

        TrackMetadataParser::Parameters params;
        params.artistTagDelimiters = { "/" };
        params.artistsToNotSplit = { "AC/DC" };
        TrackMetadataParser parser{ params };
        const Track track{ parser.parseTrackMetaData(testTags) };

        ASSERT_EQ(track.artists.size(), 1);
        EXPECT_EQ(track.artists[0].name, "AC/DC");
        EXPECT_EQ(track.artistDisplayName, "AC/DC");
        ASSERT_TRUE(track.medium.has_value());
        ASSERT_TRUE(track.medium->release.has_value());
        EXPECT_EQ(track.medium->release->name, "MyAlbum");
        ASSERT_EQ(track.medium->release->artists.size(), 1);
        EXPECT_EQ(track.medium->release->artists[0].name, "AC/DC");
        EXPECT_EQ(track.medium->release->artistDisplayName, "AC/DC");
    }

    TEST(TrackMetadataParser, customArtistDelimiters_whitelist_multi_artists)
    {
        const TestTagReader testTags{
            {
                { audio::TagType::Artist, { "AC/DC and MyArtist" } },
                { audio::TagType::Artists, { "AC/DC", "MyArtist" } },
            }
        };

        TrackMetadataParser::Parameters params;
        params.artistTagDelimiters = { "/" };
        params.artistsToNotSplit = { "  AC/DC " };
        TrackMetadataParser parser{ params };
        const Track track{ parser.parseTrackMetaData(testTags) };

        ASSERT_EQ(track.artists.size(), 2);
        EXPECT_EQ(track.artists[0].name, "AC/DC");
        EXPECT_EQ(track.artists[1].name, "MyArtist");
        EXPECT_EQ(track.artistDisplayName, "AC/DC, MyArtist"); // Reconstructed since this use case is not handled
    }

    TEST(TrackMetadataParser, customArtistDelimiters_whitelist_multi_separators_first)
    {
        const TestTagReader testTags{
            {
                { audio::TagType::Artist, { "AC/DC;MyArtist" } },
            }
        };

        TrackMetadataParser::Parameters params;
        params.artistTagDelimiters = { "/", ";" };
        params.artistsToNotSplit = { "AC/DC" };
        TrackMetadataParser parser{ params };
        const Track track{ parser.parseTrackMetaData(testTags) };

        ASSERT_EQ(track.artists.size(), 2);
        EXPECT_EQ(track.artists[0].name, "AC/DC");
        EXPECT_EQ(track.artists[1].name, "MyArtist");
        EXPECT_EQ(track.artistDisplayName, "AC/DC, MyArtist"); // Reconstructed since this use case is not handled
    }

    TEST(TrackMetadataParser, customArtistDelimiters_whitelist_multi_separators_middle)
    {
        const TestTagReader testTags{
            {
                { audio::TagType::Artist, { " MyArtist1; AC/DC  ; MyArtist2   " } },
            }
        };

        TrackMetadataParser::Parameters params;
        params.artistTagDelimiters = { "/", ";" };
        params.artistsToNotSplit = { "AC/DC" };
        TrackMetadataParser parser{ params };
        const Track track{ parser.parseTrackMetaData(testTags) };

        ASSERT_EQ(track.artists.size(), 3);
        EXPECT_EQ(track.artists[0].name, "MyArtist1");
        EXPECT_EQ(track.artists[1].name, "AC/DC");
        EXPECT_EQ(track.artists[2].name, "MyArtist2");
        EXPECT_EQ(track.artistDisplayName, "MyArtist1, AC/DC, MyArtist2"); // Reconstructed since this use case is not handled
    }

    TEST(TrackMetadataParser, customArtistDelimiters_whitelist_multi_separators_last)
    {
        const TestTagReader testTags{
            {
                { audio::TagType::Artist, { "  AC/DC; MyArtist" } },
            }
        };

        TrackMetadataParser::Parameters params;
        params.artistTagDelimiters = { ";", "/" };
        params.artistsToNotSplit = { "AC/DC" };
        TrackMetadataParser parser{ params };
        const Track track{ parser.parseTrackMetaData(testTags) };

        ASSERT_EQ(track.artists.size(), 2);
        EXPECT_EQ(track.artists[0].name, "AC/DC");
        EXPECT_EQ(track.artists[1].name, "MyArtist");
        EXPECT_EQ(track.artistDisplayName, "AC/DC, MyArtist"); // Reconstructed since this use case is not handled
    }

    TEST(TrackMetadataParser, customArtistDelimiters_whitelist_longest_first)
    {
        const TestTagReader testTags{
            {
                { audio::TagType::Artist, { "  AC/DC; MyArtist" } },
            }
        };

        TrackMetadataParser::Parameters params;
        params.artistTagDelimiters = { ";", "/" };
        params.artistsToNotSplit = { "AC", "DC", "AC/DC" };
        TrackMetadataParser parser{ params };
        const Track track{ parser.parseTrackMetaData(testTags) };

        ASSERT_EQ(track.artists.size(), 2);
        EXPECT_EQ(track.artists[0].name, "AC/DC");
        EXPECT_EQ(track.artists[1].name, "MyArtist");
        EXPECT_EQ(track.artistDisplayName, "AC/DC, MyArtist"); // Reconstructed since this use case is not handled
    }

    TEST(TrackMetadataParser, customArtistDelimiters_whitelist_partial_begin)
    {
        const TestTagReader testTags{
            {
                { audio::TagType::Artist, { "  AC/DC; MyArtist" } },
            }
        };

        TrackMetadataParser::Parameters params;
        params.artistTagDelimiters = { "/" };
        params.artistsToNotSplit = { "AC/DC" };
        TrackMetadataParser parser{ params };
        const Track track{ parser.parseTrackMetaData(testTags) };

        ASSERT_EQ(track.artists.size(), 1);
        EXPECT_EQ(track.artists[0].name, "AC/DC; MyArtist");
        EXPECT_EQ(track.artistDisplayName, "AC/DC; MyArtist");
    }

    TEST(TrackMetadataParser, customArtistDelimiters_whitelist_partial_middle)
    {
        const TestTagReader testTags{
            {
                { audio::TagType::Artist, { "  MyArtist1;  AC/DC ; MyArtist2" } },
            }
        };

        TrackMetadataParser::Parameters params;
        params.artistTagDelimiters = { "/" };
        params.artistsToNotSplit = { "AC/DC" };
        TrackMetadataParser parser{ params };
        const Track track{ parser.parseTrackMetaData(testTags) };

        ASSERT_EQ(track.artists.size(), 1);
        EXPECT_EQ(track.artists[0].name, "MyArtist1;  AC/DC ; MyArtist2");
        EXPECT_EQ(track.artistDisplayName, "MyArtist1;  AC/DC ; MyArtist2");
    }

    TEST(TrackMetadataParser, customArtistDelimiters_whitelist_partial_end)
    {
        const TestTagReader testTags{
            {
                { audio::TagType::Artist, { "  MyArtist;  AC/DC " } },
            }
        };

        TrackMetadataParser::Parameters params;
        params.artistTagDelimiters = { "/" };
        params.artistsToNotSplit = { "AC/DC" };
        TrackMetadataParser parser{ params };
        const Track track{ parser.parseTrackMetaData(testTags) };

        ASSERT_EQ(track.artists.size(), 1);
        EXPECT_EQ(track.artists[0].name, "MyArtist;  AC/DC");
        EXPECT_EQ(track.artistDisplayName, "MyArtist;  AC/DC");
    }

    TEST(TrackMetadataParser, customDelimiters_foundInArtist)
    {
        const TestTagReader testTags{
            {
                { audio::TagType::Artist, { "Artist1; Artist2" } },
                { audio::TagType::Artists, { "Artist1", "Artist2" } },
            }
        };

        TrackMetadataParser::Parameters params;
        params.artistTagDelimiters = { "; " };
        TrackMetadataParser parser{ params };

        const Track track{ parser.parseTrackMetaData(testTags) };

        ASSERT_EQ(track.artists.size(), 2);
        EXPECT_EQ(track.artists[0].name, "Artist1");
        EXPECT_EQ(track.artists[1].name, "Artist2");
        EXPECT_EQ(track.artistDisplayName, "Artist1, Artist2"); // reconstruct the display name since we hit a custom delimiter in Artist
    }

    TEST(TrackMetadataParser, customDelimiters_foundInArtists)
    {
        const TestTagReader testTags{
            {
                { audio::TagType::Artist, { "Artist1 feat. Artist2" } },
                { audio::TagType::Artists, { "Artist1; Artist2" } },
            }
        };

        TrackMetadataParser::Parameters params;
        params.artistTagDelimiters = { "; " };
        TrackMetadataParser parser{ params };

        const Track track{ parser.parseTrackMetaData(testTags) };

        ASSERT_EQ(track.artists.size(), 2);
        EXPECT_EQ(track.artists[0].name, "Artist1");
        EXPECT_EQ(track.artists[1].name, "Artist2");
        EXPECT_EQ(track.artistDisplayName, "Artist1 feat. Artist2");
    }

    TEST(TrackMetadataParser, customDelimiters_notUsed)
    {
        const TestTagReader testTags{
            {
                { audio::TagType::Artist, { "Artist1 & Artist2" } },
                { audio::TagType::Artists, { "Artist1", "Artist2" } },
            }
        };

        TrackMetadataParser::Parameters params;
        params.artistTagDelimiters = { "; " };
        TrackMetadataParser parser{ params };

        const Track track{ parser.parseTrackMetaData(testTags) };

        ASSERT_EQ(track.artists.size(), 2);
        EXPECT_EQ(track.artists[0].name, "Artist1");
        EXPECT_EQ(track.artists[1].name, "Artist2");
        EXPECT_EQ(track.artistDisplayName, "Artist1 & Artist2");
    }

    TEST(TrackMetadataParser, customDelimiters_onlyInArtist)
    {
        const TestTagReader testTags{
            {
                { audio::TagType::Artist, { "Artist1 & Artist2" } },
            }
        };

        TrackMetadataParser::Parameters params;
        params.artistTagDelimiters = { " & " };
        TrackMetadataParser parser{ params };

        const Track track{ parser.parseTrackMetaData(testTags) };

        ASSERT_EQ(track.artists.size(), 2);
        EXPECT_EQ(track.artists[0].name, "Artist1");
        EXPECT_EQ(track.artists[1].name, "Artist2");
        EXPECT_EQ(track.artistDisplayName, "Artist1, Artist2"); // reconstructed since a custom delimiter was hit for parsing
    }

    TEST(TrackMetadataParser, customDelimitersUsedForArtists)
    {
        const TestTagReader testTags{
            {
                { audio::TagType::Artists, { "Artist1 & Artist2" } },
            }
        };

        TrackMetadataParser::Parameters params;
        params.artistTagDelimiters = { " & " };
        TrackMetadataParser parser{ params };

        const Track track{ parser.parseTrackMetaData(testTags) };

        ASSERT_EQ(track.artists.size(), 2);
        EXPECT_EQ(track.artists[0].name, "Artist1");
        EXPECT_EQ(track.artists[1].name, "Artist2");
        EXPECT_EQ(track.artistDisplayName, "Artist1, Artist2"); // reconstructed since a custom delimiter was hit for parsing
    }

    TEST(TrackMetadataParser, noArtistInArtist)
    {
        const TestTagReader testTags{
            {
                // nothing in Artist!
            }
        };

        const Track track{ TrackMetadataParser{}.parseTrackMetaData(testTags) };

        ASSERT_EQ(track.artists.size(), 0);
        EXPECT_EQ(track.artistDisplayName, "");
    }

    TEST(TrackMetadataParser, singleArtistInArtists)
    {
        const TestTagReader testTags{
            {
                // nothing in Artist!
                { audio::TagType::Artists, { "Artist1" } },
            }
        };

        const Track track{ TrackMetadataParser{}.parseTrackMetaData(testTags) };

        ASSERT_EQ(track.artists.size(), 1);
        EXPECT_EQ(track.artists[0].name, "Artist1");
        EXPECT_EQ(track.artistDisplayName, "Artist1");
    }

    TEST(TrackMetadataParser, multipleArtistsInArtist)
    {
        const TestTagReader testTags{
            {
                // nothing in Artists!
                { audio::TagType::Artist, { "Artist1", "Artist2" } },
            }
        };

        const Track track{ TrackMetadataParser{}.parseTrackMetaData(testTags) };

        ASSERT_EQ(track.artists.size(), 2);
        EXPECT_EQ(track.artists[0].name, "Artist1");
        EXPECT_EQ(track.artists[1].name, "Artist2");
        EXPECT_EQ(track.artistDisplayName, "Artist1, Artist2"); // reconstruct artist display name since multiple entries are found
    }

    TEST(TrackMetadataParser, multipleArtistsInArtists)
    {
        const TestTagReader testTags{
            {
                // nothing in Artist!
                { audio::TagType::Artists, { "Artist1", "Artist2" } },
            }
        };

        const Track track{ TrackMetadataParser{}.parseTrackMetaData(testTags) };

        ASSERT_EQ(track.artists.size(), 2);
        EXPECT_EQ(track.artists[0].name, "Artist1");
        EXPECT_EQ(track.artists[1].name, "Artist2");
        EXPECT_EQ(track.artistDisplayName, "Artist1, Artist2"); // reconstruct artist display name since multiple entries are found and nothing is set in artist
    }

    TEST(TrackMetadataParser, multipleArtistsInArtistsWithEndDelimiter)
    {
        const TestTagReader testTags{
            {
                { audio::TagType::Artist, { "Artist1 & (CV. Artist2)" } },
                { audio::TagType::Artists, { "Artist1", "Artist2" } },
            }
        };

        const Track track{ TrackMetadataParser{}.parseTrackMetaData(testTags) };

        ASSERT_EQ(track.artists.size(), 2);
        EXPECT_EQ(track.artists[0].name, "Artist1");
        EXPECT_EQ(track.artists[1].name, "Artist2");
        EXPECT_EQ(track.artistDisplayName, "Artist1 & (CV. Artist2)");
    }

    TEST(TrackMetadataParser, singleArtistInAlbumArtists)
    {
        const TestTagReader testTags{
            {
                // nothing in AlbumArtist!
                { audio::TagType::Album, { "MyAlbum" } },
                { audio::TagType::AlbumArtists, { "Artist1" } },
            }
        };

        const Track track{ TrackMetadataParser{}.parseTrackMetaData(testTags) };

        ASSERT_TRUE(track.medium);
        ASSERT_TRUE(track.medium->release);
        ASSERT_EQ(track.medium->release->artists.size(), 1);
        EXPECT_EQ(track.medium->release->artists[0].name, "Artist1");
        EXPECT_EQ(track.medium->release->artistDisplayName, "Artist1");
    }

    TEST(TrackMetadataParser, multipleArtistsInAlbumArtist)
    {
        const TestTagReader testTags{
            {
                // nothing in AlbumArtists!
                { audio::TagType::Album, { "MyAlbum" } },
                { audio::TagType::AlbumArtist, { "Artist1", "Artist2" } },
            }
        };

        const Track track{ TrackMetadataParser{}.parseTrackMetaData(testTags) };

        ASSERT_TRUE(track.medium);
        ASSERT_TRUE(track.medium->release);
        ASSERT_EQ(track.medium->release->artists.size(), 2);
        EXPECT_EQ(track.medium->release->artists[0].name, "Artist1");
        EXPECT_EQ(track.medium->release->artists[1].name, "Artist2");
        EXPECT_EQ(track.medium->release->artistDisplayName, "Artist1, Artist2"); // reconstruct artist display name since multiple entries are found
    }

    TEST(TrackMetadataParser, multipleArtistsInAlbumArtists_displayName)
    {
        const TestTagReader testTags{
            {
                { audio::TagType::Album, { "MyAlbum" } },
                { audio::TagType::AlbumArtist, { "Artist1 & Artist2" } },
                { audio::TagType::AlbumArtists, { "Artist1", "Artist2" } },
            }
        };

        const Track track{ TrackMetadataParser{}.parseTrackMetaData(testTags) };

        ASSERT_TRUE(track.medium);
        ASSERT_TRUE(track.medium->release);
        ASSERT_EQ(track.medium->release->artists.size(), 2);
        EXPECT_EQ(track.medium->release->artists[0].name, "Artist1");
        EXPECT_EQ(track.medium->release->artists[1].name, "Artist2");
        EXPECT_EQ(track.medium->release->artistDisplayName, "Artist1 & Artist2");
    }

    TEST(TrackMetadataParser, multipleArtistsInAlbumArtists)
    {
        const TestTagReader testTags{
            {
                // nothing in AlbumArtist!
                { audio::TagType::Album, { "MyAlbum" } },
                { audio::TagType::AlbumArtists, { "Artist1", "Artist2" } },
            }
        };

        const Track track{ TrackMetadataParser{}.parseTrackMetaData(testTags) };

        ASSERT_TRUE(track.medium);
        ASSERT_TRUE(track.medium->release);
        ASSERT_EQ(track.medium->release->artists.size(), 2);
        EXPECT_EQ(track.medium->release->artists[0].name, "Artist1");
        EXPECT_EQ(track.medium->release->artists[1].name, "Artist2");
        EXPECT_EQ(track.medium->release->artistDisplayName, "Artist1, Artist2"); // reconstruct artist display name since multiple entries are found and nothing is set in artist
    }

    TEST(TrackMetadataParser, multipleArtistsInArtistsButNotAllMBIDs)
    {
        const TestTagReader testTags{
            {
                { audio::TagType::Artist, { "Artist1 & Artist2" } },
                { audio::TagType::Artists, { "Artist1", "Artist2" } },
                { audio::TagType::MusicBrainzArtistID, { "dd2180a2-a350-4012-b332-5d66102fa2c6" } }, // only one => no mbid will be added
            }
        };

        const Track track{ TrackMetadataParser{}.parseTrackMetaData(testTags) };

        ASSERT_EQ(track.artists.size(), 2);
        EXPECT_EQ(track.artists[0].name, "Artist1");
        EXPECT_EQ(track.artists[0].mbid, std::nullopt);
        EXPECT_EQ(track.artists[1].name, "Artist2");
        EXPECT_EQ(track.artists[1].mbid, std::nullopt);
        EXPECT_EQ(track.artistDisplayName, "Artist1 & Artist2");
    }

    TEST(TrackMetadataParser, multipleArtistsInArtistsButNotAllMBIDs_customDelimiters)
    {
        const TestTagReader testTags{
            {
                { audio::TagType::Artist, { "Artist1 / Artist2" } },
                { audio::TagType::MusicBrainzArtistID, { "dd2180a2-a350-4012-b332-5d66102fa2c6" } }, // only one => no mbid will be added
            }
        };

        TrackMetadataParser::Parameters params;
        params.artistTagDelimiters = { " / " };
        TrackMetadataParser parser{ params };
        const Track track{ parser.parseTrackMetaData(testTags) };

        ASSERT_EQ(track.artists.size(), 2);
        EXPECT_EQ(track.artists[0].name, "Artist1");
        EXPECT_EQ(track.artists[0].mbid, std::nullopt);
        EXPECT_EQ(track.artists[1].name, "Artist2");
        EXPECT_EQ(track.artists[1].mbid, std::nullopt);
        EXPECT_EQ(track.artistDisplayName, "Artist1, Artist2"); // reconstruct the artist display name
    }

    TEST(TrackMetadataParser, release_sortNameFallback)
    {
        const TestTagReader testTags{
            {
                { audio::TagType::Album, { "MyAlbum" } },
                // No AlbumSortOrder
            }
        };
        const Track track{ TrackMetadataParser{}.parseTrackMetaData(testTags) };

        ASSERT_TRUE(track.medium.has_value());
        ASSERT_TRUE(track.medium->release.has_value());
        EXPECT_EQ(track.medium->release->sortName, "MyAlbum");
    }

    TEST(TrackMetadataParser, artist_sortNameFallback)
    {
        {
            const TestTagReader testTags{
                {
                    { audio::TagType::Artist, { "MyArtist" } },
                    { audio::TagType::ArtistSortOrder, { "MyArtistSortName" } },
                    // No ArtistSortOrder
                }
            };
            const Track track{ TrackMetadataParser{}.parseTrackMetaData(testTags) };

            ASSERT_EQ(track.artists.size(), 1);
            EXPECT_EQ(track.artists[0].sortName, "MyArtistSortName");
        }

        {
            const TestTagReader testTags{
                {
                    { audio::TagType::Artist, { "MyArtist" } },
                    { audio::TagType::ArtistsSortOrder, { "MyArtistSortName" } },
                    // No ArtistSortOrder
                }
            };
            const Track track{ TrackMetadataParser{}.parseTrackMetaData(testTags) };

            ASSERT_EQ(track.artists.size(), 1);
            EXPECT_EQ(track.artists[0].sortName, "MyArtistSortName");
        }

        {
            const TestTagReader testTags{
                {
                    { audio::TagType::Artist, { "MyArtist" } },
                    { audio::TagType::ArtistSortOrder, { "MyArtistSortNameNotUsed" } },
                    { audio::TagType::ArtistsSortOrder, { "MyArtistSortName" } },
                    // No ArtistSortOrder
                }
            };
            const Track track{ TrackMetadataParser{}.parseTrackMetaData(testTags) };

            ASSERT_EQ(track.artists.size(), 1);
            EXPECT_EQ(track.artists[0].sortName, "MyArtistSortName");
        }
    }

    TEST(TrackMetadataParser, albumartist_sortNameFallback)
    {
        {
            const TestTagReader testTags{
                {
                    { audio::TagType::Album, { "MyAlbum" } },
                    { audio::TagType::AlbumArtist, { "MyArtist" } },
                    { audio::TagType::AlbumArtistSortOrder, { "MyArtistSortName" } },
                    // No ArtistSortOrder
                }
            };
            const Track track{ TrackMetadataParser{}.parseTrackMetaData(testTags) };

            ASSERT_TRUE(track.medium.has_value());
            ASSERT_TRUE(track.medium->release.has_value());

            const auto& artists{ track.medium->release->artists };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists[0].sortName, "MyArtistSortName");
        }

        {
            const TestTagReader testTags{
                {
                    { audio::TagType::Album, { "MyAlbum" } },
                    { audio::TagType::AlbumArtist, { "MyArtist" } },
                    { audio::TagType::AlbumArtistsSortOrder, { "MyArtistSortName" } },
                    // No ArtistSortOrder
                }
            };
            const Track track{ TrackMetadataParser{}.parseTrackMetaData(testTags) };

            ASSERT_TRUE(track.medium.has_value());
            ASSERT_TRUE(track.medium->release.has_value());

            const auto& artists{ track.medium->release->artists };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists[0].sortName, "MyArtistSortName");
        }

        {
            const TestTagReader testTags{
                {
                    { audio::TagType::Album, { "MyAlbum" } },

                    { audio::TagType::AlbumArtist, { "MyArtist" } },
                    { audio::TagType::AlbumArtistSortOrder, { "MyArtistSortNameNotUsed" } },
                    { audio::TagType::AlbumArtistsSortOrder, { "MyArtistSortName" } },
                    // No ArtistSortOrder
                }
            };
            const Track track{ TrackMetadataParser{}.parseTrackMetaData(testTags) };

            ASSERT_TRUE(track.medium.has_value());
            ASSERT_TRUE(track.medium->release.has_value());

            const auto& artists{ track.medium->release->artists };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists[0].sortName, "MyArtistSortName");
        }
    }

    TEST(TrackMetadataParser, advisory)
    {
        auto doTest = [](std::string_view value, std::optional<Track::Advisory> expectedValue) {
            const TestTagReader testTags{
                {
                    { audio::TagType::Advisory, { value } },
                }
            };

            const Track track{ TrackMetadataParser{}.parseTrackMetaData(testTags) };

            ASSERT_EQ(track.advisory.has_value(), expectedValue.has_value()) << "Value = '" << value << "'";
            if (track.advisory.has_value())
            {
                EXPECT_EQ(track.advisory.value(), expectedValue);
            }
        };

        doTest("0", Track::Advisory::Unknown);
        doTest("1", Track::Advisory::Explicit);
        doTest("4", Track::Advisory::Explicit);
        doTest("2", Track::Advisory::Clean);
        doTest("", std::nullopt);
        doTest("3", std::nullopt);
    }

    TEST(TrackMetadataParser, encodingTime)
    {
        auto doTest = [](std::string_view value, core::PartialDateTime expectedValue) {
            const TestTagReader testTags{
                {
                    { audio::TagType::EncodingTime, { value } },
                }
            };

            const Track track{ TrackMetadataParser{}.parseTrackMetaData(testTags) };

            ASSERT_EQ(track.encodingTime, expectedValue) << "Value = '" << value << "'";
        };

        doTest("", core::PartialDateTime{});
        doTest("foo", core::PartialDateTime{});
        doTest("2020-01-03T09:08:11.075", core::PartialDateTime{ 2020, 01, 03, 9, 8, 11 });
        doTest("2020-01-03", core::PartialDateTime{ 2020, 01, 03 });
        doTest("2020/01/03", core::PartialDateTime{ 2020, 01, 03 });
    }

    TEST(TrackMetadataParser, date)
    {
        auto doTest = [](std::string_view value, core::PartialDateTime expectedValue) {
            const TestTagReader testTags{
                {
                    { audio::TagType::Date, { value } },
                }
            };

            const Track track{ TrackMetadataParser{}.parseTrackMetaData(testTags) };

            ASSERT_EQ(track.date, expectedValue) << "Value = '" << value << "'";
        };

        doTest("", core::PartialDateTime{});
        doTest("foo", core::PartialDateTime{});
        doTest("2020-01-03", core::PartialDateTime{ 2020, 01, 03 });
        doTest("2020-01", core::PartialDateTime{ 2020, 1 });
        doTest("2020", core::PartialDateTime{ 2020 });
        doTest("2020/01/03", core::PartialDateTime{ 2020, 01, 03 });
        doTest("2020/01", core::PartialDateTime{ 2020, 1 });
        doTest("2020", core::PartialDateTime{ 2020 });
    }
} // namespace lms::scanner::tests
