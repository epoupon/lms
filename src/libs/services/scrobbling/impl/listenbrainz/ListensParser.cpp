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

#include "ListensParser.hpp"

#include <Wt/Json/Array.h>
#include <Wt/Json/Object.h>
#include <Wt/Json/Value.h>
#include <Wt/Json/Parser.h>

#include "utils/Logger.hpp"
#include "Utils.hpp"

namespace
{
	using namespace Scrobbling::ListenBrainz;

	Listen
	parseListen(const Wt::Json::Object& listenObject)
	{
		Listen listen;

		// Mandatory fields
		const Wt::Json::Object& metadata = listenObject.get("track_metadata");
		listen.trackName = static_cast<std::string>(metadata.get("track_name"));
		listen.artistName = static_cast<std::string>(metadata.get("artist_name"));

		// Optional fields
		listen.releaseName = static_cast<std::string>(metadata.get("release_name").orIfNull(""));
		if (listenObject.type("listened_at") == Wt::Json::Type::Number)
			listen.listenedAt = Wt::WDateTime::fromTime_t(static_cast<int>(listenObject.get("listened_at")));
		if (!listen.listenedAt.isValid())
			LOG(ERROR) << "Invalid or missing 'listened_at' field!";

		if (metadata.type("additional_info") == Wt::Json::Type::Object)
		{
			const Wt::Json::Object& additionalInfo = metadata.get("additional_info");
			listen.trackMBID = UUID::fromString(additionalInfo.get("track_mbid").orIfNull(""));
			listen.recordingMBID = UUID::fromString(additionalInfo.get("recording_mbid").orIfNull(""));
			listen.releaseMBID = UUID::fromString(additionalInfo.get("release_mbid").orIfNull(""));

			// tracknumber should be an integer but some players encode as strings
			int trackNumber {additionalInfo.get("tracknumber").toNumber().orIfNull(-1)};
			if (trackNumber > 0)
				listen.trackNumber = trackNumber;
		}

		return listen;
	}
} // namespace

namespace Scrobbling::ListenBrainz
{
	ListensParser::Result
	ListensParser::parse(std::string_view msgBody)
	{
		Result result;

		try
		{
			Wt::Json::Object root;
			Wt::Json::parse(std::string {msgBody}, root);

			const Wt::Json::Object& payload = root.get("payload");
			const Wt::Json::Array& listens = payload.get("listens");

			LOG(DEBUG) << "Parsing " << listens.size() << " listens...";
			result.listenCount = listens.size();

			if (listens.empty())
				return result;

			for (const Wt::Json::Value& value : listens)
			{
				try
				{
					const Wt::Json::Object& listen = value;
					result.listens.push_back(parseListen(listen));
				}
				catch (const Wt::WException& error)
				{
					LOG(ERROR) << "Cannot parse 'listen': " << error.what();
				}
			}
		}
		catch (const Wt::WException& error)
		{
			LOG(ERROR) << "Cannot parse 'listens': " << error.what();
		}

		return result;
	}
} // Scrobbling::ListenBrainz
