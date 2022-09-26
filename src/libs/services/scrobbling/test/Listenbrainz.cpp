/*
 * Copyright (C) 2022 Emeric Poupon
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

#include "listenbrainz/ListensParser.hpp"

using namespace Scrobbling::ListenBrainz;

TEST(Listenbrainz, parseListens_empty)
{
	std::vector<ListensParser::Entry> entries {ListensParser::parse("")};
	EXPECT_EQ(entries.size(), 0);

	entries = ListensParser::parse(R"({"payload":{"count":0,"latest_listen_ts":1664105200,"listens":[],"user_id":"epoupon"}})");
	EXPECT_EQ(entries.size(), 0);
}

TEST(Listenbrainz, parseListens_single_missingMBID)
{
	std::vector<ListensParser::Entry> entries {ListensParser::parse(R"({"payload":{"count":1,"latest_listen_ts":1663159479,"listens":[{"inserted_at":1650541124,"listened_at":1650541124,"recording_msid":"0e1418e3-b485-413a-84af-6316312cb116","track_metadata":{"additional_info":{"artist_msid":"ab5b27ad-e579-441c-ac60-d5dd9975c044","listening_from":"LMS","recording_msid":"0e1418e3-b485-413a-84af-6316312cb116","release_msid":"3f22f274-a9ee-4cb2-8dd1-f3bd18407099","tracknumber":8},"artist_name":"Broke For Free","release_name":"YEKOMS","track_name":"U2B"},"user_name":"epoupon"}],"user_id":"epoupon"}})")};
	ASSERT_EQ(entries.size(), 1);
	EXPECT_EQ(entries[0].trackName, "U2B");
	EXPECT_EQ(entries[0].releaseName, "YEKOMS");
	EXPECT_EQ(entries[0].artistName, "Broke For Free");
	EXPECT_EQ(entries[0].recordingMBID, std::nullopt);
	EXPECT_EQ(entries[0].releaseMBID, std::nullopt);
	EXPECT_EQ(entries[0].trackNumber, 8);

	Wt::WDateTime listenedAt;
	listenedAt.setTime_t(1650541124);
	EXPECT_EQ(entries[0].listenedAt, listenedAt);
}

TEST(Listenbrainz, parseListens_twoEntries)
{
	std::vector<ListensParser::Entry> entries {ListensParser::parse(R"({"payload":{"count":2,"latest_listen_ts":1664028167,"listens":[{"inserted_at":1664028167,"listened_at":1664028167,"recording_msid":"29c11137-e40b-4875-9ec0-9a20a4bdc2d3","track_metadata":{"additional_info":{"artist_mbids":["069a1c1f-14eb-4d36-b0a0-77dffbd67713"],"artist_msid":null,"listening_from":"LMS","recording_mbid":"46ae879f-2dbe-46d3-99ad-05c116f97a30","recording_msid":"29c11137-e40b-4875-9ec0-9a20a4bdc2d3","release_mbid":"44915500-fbb9-4060-98ce-59a57a429edc","release_msid":null,"track_mbid":"5427a943-a096-4d0b-8b9a-53aca9ed61ac","tracknumber":5},"artist_name":"Broke For Free","mbid_mapping":{"artist_mbids":["069a1c1f-14eb-4d36-b0a0-77dffbd67713"],"recording_mbid":"46ae879f-2dbe-46d3-99ad-05c116f97a30","release_mbid":"44915500-fbb9-4060-98ce-59a57a429edc"},"release_name":"Petal","track_name":"Juparo"},"user_name":"epoupon"},{"inserted_at":1664027919,"listened_at":1664027918,"recording_msid":"fe5abc47-89cd-4235-80b5-00f47cecbe01","track_metadata":{"additional_info":{"artist_mbids":["069a1c1f-14eb-4d36-b0a0-77dffbd67713"],"artist_msid":null,"listening_from":"LMS","recording_mbid":"d89d042c-8cc1-4526-9080-5bab728ee15f","recording_msid":"fe5abc47-89cd-4235-80b5-00f47cecbe01","release_mbid":"44915500-fbb9-4060-98ce-59a57a429edc","release_msid":null,"track_mbid":"9f33a17f-e33e-492f-85a4-7b2e9e09613e","tracknumber":4},"artist_name":"Broke For Free","mbid_mapping":{"artist_mbids":["069a1c1f-14eb-4d36-b0a0-77dffbd67713"],"recording_mbid":"d89d042c-8cc1-4526-9080-5bab728ee15f","release_mbid":"44915500-fbb9-4060-98ce-59a57a429edc"},"release_name":"Petal","track_name":"Melt"},"user_name":"epoupon"}],"user_id":"epoupon"}})")};
	ASSERT_EQ(entries.size(), 2);
	EXPECT_EQ(entries[0].trackName, "Juparo");
	EXPECT_EQ(entries[0].releaseName, "Petal");
	EXPECT_EQ(entries[0].artistName, "Broke For Free");
	ASSERT_TRUE(entries[0].recordingMBID.has_value());
	EXPECT_EQ(entries[0].recordingMBID->getAsString(), "46ae879f-2dbe-46d3-99ad-05c116f97a30");
	ASSERT_TRUE(entries[0].releaseMBID.has_value());
	EXPECT_EQ(entries[0].releaseMBID->getAsString(), "44915500-fbb9-4060-98ce-59a57a429edc");
	EXPECT_EQ(entries[0].trackNumber, 5);

	EXPECT_EQ(entries[1].trackName, "Melt");
	EXPECT_EQ(entries[1].releaseName, "Petal");
	EXPECT_EQ(entries[1].artistName, "Broke For Free");
	ASSERT_TRUE(entries[1].recordingMBID.has_value());
	EXPECT_EQ(entries[1].recordingMBID->getAsString(), "d89d042c-8cc1-4526-9080-5bab728ee15f");
	ASSERT_TRUE(entries[1].releaseMBID.has_value());
	EXPECT_EQ(entries[1].releaseMBID->getAsString(), "44915500-fbb9-4060-98ce-59a57a429edc");
	EXPECT_EQ(entries[1].trackNumber, 4);
}

TEST(Listenbrainz, parseListens_entryNotFromLms)
{
	std::vector<ListensParser::Entry> entries {ListensParser::parse(R"({"payload":{"count":1,"latest_listen_ts":1664105730,"listens":[{"inserted_at":1664105730,"listened_at":1664105730,"recording_msid":"6a11ff4d-0623-4b2e-98e0-0e172f1f28d7","track_metadata":{"additional_info":{"artist_msid":null,"media_player":"BrainzPlayer","music_service":"youtube.com","music_service_name":"youtube","origin_url":"https://www.youtube.com/watch?v=EBP5vL3YWTI","recording_msid":"6a11ff4d-0623-4b2e-98e0-0e172f1f28d7","release_msid":null,"submission_client":"BrainzPlayer"},"artist_name":"Dio","brainzplayer_metadata":{"track_name":"Dio - Breathless"},"mbid_mapping":{"artist_mbids":["c55193fb-f5d2-4839-a263-4c044fca1456"],"recording_mbid":"92929526-21d7-4e75-b759-1072951664c4","release_mbid":"16cbf9ba-2e38-3893-9f23-f8567e26c18b"},"release_name":"The Last in Line","track_name":"Breathless"},"user_name":"epoupon"}],"user_id":"epoupon"}})")};
	ASSERT_EQ(entries.size(), 1);
	EXPECT_EQ(entries[0].trackName, "Breathless");
	EXPECT_EQ(entries[0].releaseName, "The Last in Line");
	EXPECT_EQ(entries[0].artistName, "Dio");
	EXPECT_FALSE(entries[0].recordingMBID.has_value());
	EXPECT_FALSE(entries[0].releaseMBID.has_value());
}

TEST(Listenbrainz, parseListens_multiArtists)
{
	std::vector<ListensParser::Entry> entries {ListensParser::parse(R"({"payload":{"count":1,"latest_listen_ts":1664106427,"listens":[{"inserted_at":1664106427,"listened_at":1664106427,"recording_msid":"b1dad0df-329b-443d-bacf-cdbebdddbfd0","track_metadata":{"additional_info":{"artist_mbids":["04ce0202-043d-4cbe-8f09-8abaf3b80c71","79311c51-9748-49df-baa1-d925fd29f4e8"],"artist_msid":null,"listening_from":"LMS","recording_mbid":"a5f380bc-0a85-4a9f-88db-d41bb9aa2a4b","recording_msid":"b1dad0df-329b-443d-bacf-cdbebdddbfd0","release_mbid":"147b4669-3d20-43f8-89c0-ba1da8b87dd3","release_msid":null,"track_mbid":"a20dd067-29b6-3d38-a0be-eeb86b4671c1","tracknumber":1},"artist_name":"Gloom","release_name":"Demovibes 9: Party, people going","track_name":"Stargazer of Disgrace"},"user_name":"epoupon"}],"user_id":"epoupon"}})")};
	ASSERT_EQ(entries.size(), 1);
}

TEST(Listenbrainz, parseListens_minPayload)
{
	std::vector<ListensParser::Entry> entries {ListensParser::parse(R"({"payload":{"count":1,"latest_listen_ts":1664106427,"listens":[{"track_metadata":{"artist_name":"Gloom","track_name":"Stargazer of Disgrace"},"user_name":"epoupon"}],"user_id":"epoupon"}})")};
	ASSERT_EQ(entries.size(), 1);
	EXPECT_FALSE(entries[0].listenedAt.isValid());
	EXPECT_EQ(entries[0].trackName, "Stargazer of Disgrace");
	EXPECT_EQ(entries[0].artistName, "Gloom");
	EXPECT_EQ(entries[0].releaseName, "");
	EXPECT_FALSE(entries[0].recordingMBID.has_value());
	EXPECT_FALSE(entries[0].releaseMBID.has_value());
}
