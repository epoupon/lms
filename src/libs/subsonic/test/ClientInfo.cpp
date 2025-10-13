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

#include <Wt/WException.h>
#include <gtest/gtest.h>

#include "SubsonicResponse.hpp"
#include "responses/ClientInfo.hpp"

namespace lms::api::subsonic
{
    TEST(ClientInfo, basic)
    {
        std::istringstream iss{ R"({
  "name": "Play:1",
  "platform": "Sonos",
  "maxAudioBitrate": 512000,
  "maxTranscodingAudioBitrate": 256000,
  "directPlayProfiles": [
  {
"container": "mp3",
"audioCodec": "mp3",
"protocol": "http",
"maxAudioChannels": 2
  },
  {
"container": "flac",
"audioCodec": "flac",
"protocol": "*",
"maxAudioChannels": 2
  }
  ,
  {
"container": "mp4",
"audioCodec": "flac,aac,alac",
"protocol": "*",
"maxAudioChannels": 2
  }
  ],
  "transcodingProfiles": [
  {
"container": "mp3",
"audioCodec": "mp3",
"protocol": "http",
"maxAudioChannels": 2
  },
  {
"container": "flac",
"audioCodec": "flac",
"protocol": "*",
"maxAudioChannels": 2
  }
  ],
  "codecProfiles": [
    {
      "type": "AudioCodec",
      "name": "mp3",
      "limitations": [
        { "name": "audioBitrate", "comparison": "LessThanEqual", "value": "320000", "required": true }
      ]
    },
    {
      "type": "AudioCodec",
      "name": "flac",
      "limitations": [
        { "name": "audioSamplerate", "comparison": "LessThanEqual", "value": "192000", "required": false },
        { "name": "audioChannels",  "comparison": "LessThanEqual", "value": "2",      "required": false }
      ]
    }
  ]
})" };
        try
        {
            const ClientInfo clientInfo{ parseClientInfoFromJson(iss) };

            EXPECT_EQ(clientInfo.name, "Play:1");
            EXPECT_EQ(clientInfo.platform, "Sonos");
            EXPECT_TRUE(clientInfo.maxAudioBitrate.has_value());
            EXPECT_EQ(clientInfo.maxAudioBitrate.value(), 512000);
            EXPECT_TRUE(clientInfo.maxTranscodingAudioBitrate.has_value());
            EXPECT_EQ(clientInfo.maxTranscodingAudioBitrate.value(), 256000);

            ASSERT_EQ(clientInfo.directPlayProfiles.size(), 3);
            EXPECT_EQ(clientInfo.directPlayProfiles[0].container, "mp3");
            ASSERT_EQ(clientInfo.directPlayProfiles[0].audioCodecs.size(), 1);
            EXPECT_EQ(clientInfo.directPlayProfiles[0].audioCodecs[0], "mp3");
            EXPECT_EQ(clientInfo.directPlayProfiles[0].protocol, "http");
            EXPECT_EQ(clientInfo.directPlayProfiles[0].maxAudioChannels, 2);
            EXPECT_EQ(clientInfo.directPlayProfiles[1].container, "flac");
            EXPECT_EQ(clientInfo.directPlayProfiles[1].audioCodecs.size(), 1);
            EXPECT_EQ(clientInfo.directPlayProfiles[1].audioCodecs[0], "flac");
            EXPECT_EQ(clientInfo.directPlayProfiles[1].protocol, "*");
            EXPECT_EQ(clientInfo.directPlayProfiles[1].maxAudioChannels, 2);
            EXPECT_EQ(clientInfo.directPlayProfiles[2].container, "mp4");
            EXPECT_EQ(clientInfo.directPlayProfiles[2].audioCodecs.size(), 3);
            EXPECT_EQ(clientInfo.directPlayProfiles[2].audioCodecs[0], "flac");
            EXPECT_EQ(clientInfo.directPlayProfiles[2].audioCodecs[1], "aac");
            EXPECT_EQ(clientInfo.directPlayProfiles[2].audioCodecs[2], "alac");
            EXPECT_EQ(clientInfo.directPlayProfiles[2].protocol, "*");
            EXPECT_EQ(clientInfo.directPlayProfiles[2].maxAudioChannels, 2);
            ASSERT_EQ(clientInfo.transcodingProfiles.size(), 2);
            EXPECT_EQ(clientInfo.transcodingProfiles[0].container, "mp3");
            EXPECT_EQ(clientInfo.transcodingProfiles[0].audioCodec, "mp3");
            EXPECT_EQ(clientInfo.transcodingProfiles[0].protocol, "http");
            EXPECT_EQ(clientInfo.transcodingProfiles[0].maxAudioChannels, 2);
            EXPECT_EQ(clientInfo.transcodingProfiles[1].container, "flac");
            EXPECT_EQ(clientInfo.transcodingProfiles[1].audioCodec, "flac");
            EXPECT_EQ(clientInfo.transcodingProfiles[1].protocol, "*");
            EXPECT_EQ(clientInfo.transcodingProfiles[1].maxAudioChannels, 2);
            ASSERT_EQ(clientInfo.codecProfiles.size(), 2);
            EXPECT_EQ(clientInfo.codecProfiles[0].type, "AudioCodec");
            EXPECT_EQ(clientInfo.codecProfiles[0].name, "mp3");
            ASSERT_EQ(clientInfo.codecProfiles[0].limitations.size(), 1);
            EXPECT_EQ(clientInfo.codecProfiles[0].limitations[0].name, Limitation::Type::AudioBitrate);
            EXPECT_EQ(clientInfo.codecProfiles[0].limitations[0].comparison, Limitation::ComparisonOperator::LessThanEqual);
            EXPECT_EQ(clientInfo.codecProfiles[0].limitations[0].values.size(), 1);
            EXPECT_EQ(clientInfo.codecProfiles[0].limitations[0].values[0], "320000");
            EXPECT_TRUE(clientInfo.codecProfiles[0].limitations[0].required);
            EXPECT_EQ(clientInfo.codecProfiles[1].type, "AudioCodec");
            EXPECT_EQ(clientInfo.codecProfiles[1].name, "flac");
            ASSERT_EQ(clientInfo.codecProfiles[1].limitations.size(), 2);
            EXPECT_EQ(clientInfo.codecProfiles[1].limitations[0].name, Limitation::Type::AudioSamplerate);
            EXPECT_EQ(clientInfo.codecProfiles[1].limitations[0].comparison, Limitation::ComparisonOperator::LessThanEqual);
            EXPECT_EQ(clientInfo.codecProfiles[1].limitations[0].values.size(), 1);
            EXPECT_EQ(clientInfo.codecProfiles[1].limitations[0].values[0], "192000");
            EXPECT_FALSE(clientInfo.codecProfiles[1].limitations[0].required);
            EXPECT_EQ(clientInfo.codecProfiles[1].limitations[1].name, Limitation::Type::AudioChannels);
            EXPECT_EQ(clientInfo.codecProfiles[1].limitations[1].comparison, Limitation::ComparisonOperator::LessThanEqual);
            EXPECT_EQ(clientInfo.codecProfiles[1].limitations[1].values.size(), 1);
            EXPECT_EQ(clientInfo.codecProfiles[1].limitations[1].values[0], "2");
            EXPECT_FALSE(clientInfo.codecProfiles[1].limitations[1].required);
        }
        catch (const Error& e)
        {
            GTEST_FAIL() << e.getMessage();
        }
    }
} // namespace lms::api::subsonic