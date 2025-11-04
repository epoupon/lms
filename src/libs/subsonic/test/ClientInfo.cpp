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
            ASSERT_EQ(clientInfo.directPlayProfiles[0].containers.size(), 1);
            EXPECT_EQ(clientInfo.directPlayProfiles[0].containers[0], "mp3");
            ASSERT_EQ(clientInfo.directPlayProfiles[0].audioCodecs.size(), 1);
            EXPECT_EQ(clientInfo.directPlayProfiles[0].audioCodecs[0], "mp3");
            EXPECT_EQ(clientInfo.directPlayProfiles[0].protocol, "http");
            EXPECT_EQ(clientInfo.directPlayProfiles[0].maxAudioChannels, 2);
            ASSERT_EQ(clientInfo.directPlayProfiles[1].containers.size(), 1);
            EXPECT_EQ(clientInfo.directPlayProfiles[1].containers[0], "flac");
            EXPECT_EQ(clientInfo.directPlayProfiles[1].audioCodecs.size(), 1);
            EXPECT_EQ(clientInfo.directPlayProfiles[1].audioCodecs[0], "flac");
            EXPECT_EQ(clientInfo.directPlayProfiles[1].protocol, "*");
            EXPECT_EQ(clientInfo.directPlayProfiles[1].maxAudioChannels, 2);
            ASSERT_EQ(clientInfo.directPlayProfiles[2].containers.size(), 1);
            EXPECT_EQ(clientInfo.directPlayProfiles[2].containers[0], "mp4");
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

    TEST(ClientInfo, multi)
    {
        std::istringstream iss{ R"({"name":"LocalDevice","platform":"Android","maxAudioBitrate":320000,"maxTranscodingAudioBitrate":320000,"directPlayProfiles":[{"container":"mp4,mka,m4a,mp3,mp2,wav,flac,ogg,alac,opus,vorbis","audioCodec":"*","protocol":"*","maxAudioChannels":32}],"transcodingProfiles":[{"container":"flac","audioCodec":"flac","protocol":"http","maxAudioChannels":0},{"container":"ogg","audioCodec":"opus","protocol":"http","maxAudioChannels":6},{"container":"mp3","audioCodec":"mp3","protocol":"http","maxAudioChannels":2}],"codecProfiles":[{"type":"AudioCodec","name":"vorbis","limitations":[{"name":"audioSamplerate","comparison":"LessThanEqual","value":"48000","required":true}]},{"type":"AudioCodec","name":"opus","limitations":[{"name":"audioSamplerate","comparison":"LessThanEqual","value":"48000","required":true}]}]})" };

        try
        {
            const ClientInfo clientInfo{ parseClientInfoFromJson(iss) };

            EXPECT_EQ(clientInfo.name, "LocalDevice");
            EXPECT_EQ(clientInfo.platform, "Android");
            EXPECT_EQ(clientInfo.maxAudioBitrate, 320000);
            EXPECT_EQ(clientInfo.maxTranscodingAudioBitrate, 320000);
            ASSERT_EQ(clientInfo.directPlayProfiles.size(), 1);
            ASSERT_EQ(clientInfo.directPlayProfiles[0].containers.size(), 11);
            EXPECT_EQ(clientInfo.directPlayProfiles[0].containers[0], "mp4");
            EXPECT_EQ(clientInfo.directPlayProfiles[0].containers[1], "mka");
            EXPECT_EQ(clientInfo.directPlayProfiles[0].containers[2], "m4a");
            EXPECT_EQ(clientInfo.directPlayProfiles[0].containers[3], "mp3");
            EXPECT_EQ(clientInfo.directPlayProfiles[0].containers[4], "mp2");
            EXPECT_EQ(clientInfo.directPlayProfiles[0].containers[5], "wav");
            EXPECT_EQ(clientInfo.directPlayProfiles[0].containers[6], "flac");
            EXPECT_EQ(clientInfo.directPlayProfiles[0].containers[7], "ogg");
            EXPECT_EQ(clientInfo.directPlayProfiles[0].containers[8], "alac");
            EXPECT_EQ(clientInfo.directPlayProfiles[0].containers[9], "opus");
            EXPECT_EQ(clientInfo.directPlayProfiles[0].containers[10], "vorbis");
            ASSERT_EQ(clientInfo.directPlayProfiles[0].audioCodecs.size(), 1);
            EXPECT_EQ(clientInfo.directPlayProfiles[0].audioCodecs[0], "*");
            EXPECT_EQ(clientInfo.directPlayProfiles[0].protocol, "*");
            EXPECT_EQ(clientInfo.directPlayProfiles[0].maxAudioChannels, 32);
            ASSERT_EQ(clientInfo.transcodingProfiles.size(), 3);
            EXPECT_EQ(clientInfo.transcodingProfiles[0].container, "flac");
            EXPECT_EQ(clientInfo.transcodingProfiles[0].audioCodec, "flac");
            EXPECT_EQ(clientInfo.transcodingProfiles[0].protocol, "http");
            EXPECT_EQ(clientInfo.transcodingProfiles[0].maxAudioChannels, std::nullopt);
            EXPECT_EQ(clientInfo.transcodingProfiles[1].container, "ogg");
            EXPECT_EQ(clientInfo.transcodingProfiles[1].audioCodec, "opus");
            EXPECT_EQ(clientInfo.transcodingProfiles[1].protocol, "http");
            EXPECT_EQ(clientInfo.transcodingProfiles[1].maxAudioChannels, 6);
            EXPECT_EQ(clientInfo.transcodingProfiles[2].container, "mp3");
            EXPECT_EQ(clientInfo.transcodingProfiles[2].audioCodec, "mp3");
            EXPECT_EQ(clientInfo.transcodingProfiles[2].protocol, "http");
            EXPECT_EQ(clientInfo.transcodingProfiles[2].maxAudioChannels, 2);
            ASSERT_EQ(clientInfo.codecProfiles.size(), 2);
            EXPECT_EQ(clientInfo.codecProfiles[0].type, "AudioCodec");
            EXPECT_EQ(clientInfo.codecProfiles[0].name, "vorbis");
            ASSERT_EQ(clientInfo.codecProfiles[0].limitations.size(), 1);
            EXPECT_EQ(clientInfo.codecProfiles[0].limitations[0].name, Limitation::Type::AudioSamplerate);
            EXPECT_EQ(clientInfo.codecProfiles[0].limitations[0].comparison, Limitation::ComparisonOperator::LessThanEqual);
            EXPECT_EQ(clientInfo.codecProfiles[0].limitations[0].values.size(), 1);
            EXPECT_EQ(clientInfo.codecProfiles[0].limitations[0].values[0], "48000");
            EXPECT_EQ(clientInfo.codecProfiles[0].limitations[0].required, true);
            EXPECT_EQ(clientInfo.codecProfiles[1].type, "AudioCodec");
            EXPECT_EQ(clientInfo.codecProfiles[1].name, "opus");
            ASSERT_EQ(clientInfo.codecProfiles[1].limitations.size(), 1);
            EXPECT_EQ(clientInfo.codecProfiles[1].limitations[0].name, Limitation::Type::AudioSamplerate);
            EXPECT_EQ(clientInfo.codecProfiles[1].limitations[0].comparison, Limitation::ComparisonOperator::LessThanEqual);
            EXPECT_EQ(clientInfo.codecProfiles[1].limitations[0].values.size(), 1);
            EXPECT_EQ(clientInfo.codecProfiles[1].limitations[0].values[0], "48000");
            EXPECT_EQ(clientInfo.codecProfiles[1].limitations[0].required, true);
        }
        catch (const Error& e)
        {
            GTEST_FAIL() << e.getMessage();
        }
    }

    TEST(ClientInfo, multi2)
    {
        std::istringstream iss{ R"({"name":"Upnp/192.168.1.1/Foo","platform":"UPnP","maxAudioBitrate":0,"maxTranscodingAudioBitrate":0,"directPlayProfiles":[{"container":"opus,ogg,oga,aac,webma,webm,wav,flac,mka","audioCodec":"*","protocol":"*","maxAudioChannels":0},{"container":"mp3","audioCodec":"mp3","protocol":"*","maxAudioChannels":0},{"container":"m4a,mp4","audioCodec":"aac","protocol":"*","maxAudioChannels":0}],"transcodingProfiles":[{"container":"flac","audioCodec":"flac","protocol":"http","maxAudioChannels":6},{"container":"mp4","audioCodec":"aac","protocol":"http","maxAudioChannels":6},{"container":"aac","audioCodec":"aac","protocol":"http","maxAudioChannels":6},{"container":"mp3","audioCodec":"mp3","protocol":"http","maxAudioChannels":2}],"codecProfiles":[{"type":"AudioCodec","name":"flac","limitations":[{"name":"audioSamplerate","comparison":"LessThanEqual","value":"48000","required":true}]},{"type":"AudioCodec","name":"vorbis","limitations":[{"name":"audioSamplerate","comparison":"LessThanEqual","value":"48000","required":true}]},{"type":"AudioCodec","name":"opus","limitations":[{"name":"audioSamplerate","comparison":"LessThanEqual","value":"48000","required":true}]}]})" };
        try
        {
            const ClientInfo clientInfo{ parseClientInfoFromJson(iss) };

            EXPECT_EQ(clientInfo.name, "Upnp/192.168.1.1/Foo");
            EXPECT_EQ(clientInfo.platform, "UPnP");
            EXPECT_EQ(clientInfo.maxAudioBitrate, std::nullopt);
            EXPECT_EQ(clientInfo.maxTranscodingAudioBitrate, std::nullopt);
            ASSERT_EQ(clientInfo.directPlayProfiles.size(), 3);
            ASSERT_EQ(clientInfo.directPlayProfiles[0].containers.size(), 9);
            EXPECT_EQ(clientInfo.directPlayProfiles[0].containers[0], "opus");
            EXPECT_EQ(clientInfo.directPlayProfiles[0].containers[1], "ogg");
            EXPECT_EQ(clientInfo.directPlayProfiles[0].containers[2], "oga");
            EXPECT_EQ(clientInfo.directPlayProfiles[0].containers[3], "aac");
            EXPECT_EQ(clientInfo.directPlayProfiles[0].containers[4], "webma");
            EXPECT_EQ(clientInfo.directPlayProfiles[0].containers[5], "webm");
            EXPECT_EQ(clientInfo.directPlayProfiles[0].containers[6], "wav");
            EXPECT_EQ(clientInfo.directPlayProfiles[0].containers[7], "flac");
            EXPECT_EQ(clientInfo.directPlayProfiles[0].containers[8], "mka");
            ASSERT_EQ(clientInfo.directPlayProfiles[0].audioCodecs.size(), 1);
            EXPECT_EQ(clientInfo.directPlayProfiles[0].audioCodecs[0], "*");
            EXPECT_EQ(clientInfo.directPlayProfiles[0].protocol, "*");
            EXPECT_EQ(clientInfo.directPlayProfiles[0].maxAudioChannels, std::nullopt);
            ASSERT_EQ(clientInfo.transcodingProfiles.size(), 4);
            EXPECT_EQ(clientInfo.transcodingProfiles[0].container, "flac");
            EXPECT_EQ(clientInfo.transcodingProfiles[0].audioCodec, "flac");
            EXPECT_EQ(clientInfo.transcodingProfiles[0].protocol, "http");
            EXPECT_EQ(clientInfo.transcodingProfiles[0].maxAudioChannels, 6);
            EXPECT_EQ(clientInfo.transcodingProfiles[1].container, "mp4");
            EXPECT_EQ(clientInfo.transcodingProfiles[1].audioCodec, "aac");
            EXPECT_EQ(clientInfo.transcodingProfiles[1].protocol, "http");
            EXPECT_EQ(clientInfo.transcodingProfiles[1].maxAudioChannels, 6);
            EXPECT_EQ(clientInfo.transcodingProfiles[2].container, "aac");
            EXPECT_EQ(clientInfo.transcodingProfiles[2].audioCodec, "aac");
            EXPECT_EQ(clientInfo.transcodingProfiles[2].protocol, "http");
            EXPECT_EQ(clientInfo.transcodingProfiles[2].maxAudioChannels, 6);
            EXPECT_EQ(clientInfo.transcodingProfiles[3].container, "mp3");
            EXPECT_EQ(clientInfo.transcodingProfiles[3].audioCodec, "mp3");
            EXPECT_EQ(clientInfo.transcodingProfiles[3].protocol, "http");
            EXPECT_EQ(clientInfo.transcodingProfiles[3].maxAudioChannels, 2);
            ASSERT_EQ(clientInfo.codecProfiles.size(), 3);
            EXPECT_EQ(clientInfo.codecProfiles[0].type, "AudioCodec");
            EXPECT_EQ(clientInfo.codecProfiles[0].name, "flac");
            ASSERT_EQ(clientInfo.codecProfiles[0].limitations.size(), 1);
            EXPECT_EQ(clientInfo.codecProfiles[0].limitations[0].name, Limitation::Type::AudioSamplerate);
            EXPECT_EQ(clientInfo.codecProfiles[0].limitations[0].comparison, Limitation::ComparisonOperator::LessThanEqual);
            EXPECT_EQ(clientInfo.codecProfiles[0].limitations[0].values.size(), 1);
            EXPECT_EQ(clientInfo.codecProfiles[0].limitations[0].values[0], "48000");
            EXPECT_EQ(clientInfo.codecProfiles[0].limitations[0].required, true);
            EXPECT_EQ(clientInfo.codecProfiles[1].type, "AudioCodec");
            EXPECT_EQ(clientInfo.codecProfiles[1].name, "vorbis");
            ASSERT_EQ(clientInfo.codecProfiles[1].limitations.size(), 1);
            EXPECT_EQ(clientInfo.codecProfiles[1].limitations[0].name, Limitation::Type::AudioSamplerate);
            EXPECT_EQ(clientInfo.codecProfiles[1].limitations[0].comparison, Limitation::ComparisonOperator::LessThanEqual);
            EXPECT_EQ(clientInfo.codecProfiles[1].limitations[0].values.size(), 1);
            EXPECT_EQ(clientInfo.codecProfiles[1].limitations[0].values[0], "48000");
            EXPECT_EQ(clientInfo.codecProfiles[1].limitations[0].required, true);
            EXPECT_EQ(clientInfo.codecProfiles[2].type, "AudioCodec");
            EXPECT_EQ(clientInfo.codecProfiles[2].name, "opus");
            ASSERT_EQ(clientInfo.codecProfiles[2].limitations.size(), 1);
            EXPECT_EQ(clientInfo.codecProfiles[2].limitations[0].name, Limitation::Type::AudioSamplerate);
            EXPECT_EQ(clientInfo.codecProfiles[2].limitations[0].comparison, Limitation::ComparisonOperator::LessThanEqual);
            EXPECT_EQ(clientInfo.codecProfiles[2].limitations[0].values.size(), 1);
            EXPECT_EQ(clientInfo.codecProfiles[2].limitations[0].values[0], "48000");
            EXPECT_EQ(clientInfo.codecProfiles[2].limitations[0].required, true);
        }
        catch (const Error& e)
        {
            GTEST_FAIL() << e.getMessage();
        }
    }

    TEST(ClientInfo, badfield)
    {
        std::istringstream iss{ R"({"name":"LocalDevice","platform":"Android","maxAudioBitrate":"320000","maxTranscodingAudioBitrate":320000,"directPlayProfiles":[{"container":"mp4,mka,m4a,mp3,mp2,wav,flac,ogg,alac,opus,vorbis","audioCodec":"*","protocol":"*","maxAudioChannels":32}],"transcodingProfiles":[{"container":"flac","audioCodec":"flac","protocol":"http","maxAudioChannels":0},{"container":"ogg","audioCodec":"opus","protocol":"http","maxAudioChannels":6},{"container":"mp3","audioCodec":"mp3","protocol":"http","maxAudioChannels":2}],"codecProfiles":[{"type":"AudioCodec","name":"vorbis","limitations":[{"name":"audioSamplerate","comparison":"LessThanEqual","value":"48000","required":true}]},{"type":"AudioCodec","name":"opus","limitations":[{"name":"audioSamplerate","comparison":"LessThanEqual","value":"48000","required":true}]}]})" };

        try
        {
            const ClientInfo clientInfo{ parseClientInfoFromJson(iss) };
            GTEST_FAIL() << "Expected error";
        }
        catch (const BadParameterGenericError& e)
        {
            EXPECT_EQ(e.getParameterName(), "maxAudioBitrate");
        }
        catch (const Error& e)
        {
            GTEST_FAIL() << e.getMessage();
        }
    }

} // namespace lms::api::subsonic